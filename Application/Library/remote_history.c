#include "remote_history.h"
#include "main.h"
#include "define.h"
#include "config_store.h"
#include "cmsis_os.h"
#include "link_monitor.h"
#include "utility.h"
#include <string.h>

#define PENDING_SLOTS 4

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];
extern osMessageQueueId_t txMsgQueueHandle;

/* Event waiting to be written to flash by RemoteHistory_Tick.
   ready: 0 free, 1 queued, 2 being stored. */
typedef struct
{
    HistoryEvent event;
    uint16_t transaction;
    uint16_t owner;
    volatile uint8_t ready;
} RemoteHistory_Pending_t;

uint32_t remoteHistoryStored;
uint32_t remoteHistoryDuplicates;
uint32_t remoteHistoryAcked;
uint32_t remoteHistoryRejected;
uint32_t remoteHistoryRevision;

static RemoteHistory_Pending_t pending[PENDING_SLOTS];

static void enqueue(const HistoryEvent *event, uint16_t owner, uint16_t transaction);

/** @brief Initialise the journal and clear the pending queue. Call once from main(). */
void RemoteHistory_Init(void)
{
    HJ_Init();
    memset(pending, 0, sizeof(pending));
}

/**
 * @brief  Journal owner of a paired device.
 * @param  id  Device index in deviceBuff.
 * @return (netId << 8) | nodeId.
 */
uint16_t RemoteHistory_Owner(uint8_t id)
{
    return ((uint16_t)systemInfo.netId << 8) | storageData.devices[id].nodeId;
}

/**
 * @brief  Validate a CMD_HISTORY push from a device and queue its event.
 * @param  packet  Received frame.
 * @note   Called from the receive path; the event is only accepted from the
 *         paired device with the same node id and serial number.
 */
void RemoteHistory_Handle(LoraPackage_t packet)
{
    HistoryPush push;
    HistoryEvent event;

    /* Validate the frame and the event payload. */
    if (packet.type != LORA_PACKAGE_TYPE_REQUEST || packet.netId != systemInfo.netId ||
        packet.byteCount != LORA_PACKAGE_HEADER_SIZE + sizeof(HistoryPush) + 1)
    {
        remoteHistoryRejected++;
        return;
    }
    memcpy(&push, packet.rawData + LORA_PACKAGE_HEADER_SIZE, sizeof(push));
    if (push.crc != HJ_Crc(&push.event, sizeof(push.event)))
    {
        remoteHistoryRejected++;
        return;
    }
    event = push.event;
    if (!event.id || event.id == 0xFFFFFFFFu || event.action < HIST_ON || event.action > HIST_CLEAR ||
        event.source < HIST_REMOTE || event.source > HIST_SLAVE || event.before > 2 || event.after > 2 ||
        event.result < HIST_APPLIED || event.result > HIST_WARNING || (event.flags & ~HIST_TIME_UNKNOWN))
    {
        remoteHistoryRejected++;
        return;
    }

    /* Accept it only from the paired device with the same serial. */
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        if (storageData.devices[id].deleted || storageData.devices[id].nodeId != packet.nodeId ||
            storageData.devices[id].factoryInfo.serial != event.serial)
        {
            continue;
        }
        Link_RecordPacket(id, 0);
        enqueue(&event, RemoteHistory_Owner(id), packet.transactionID);
        return;
    }
    remoteHistoryRejected++;
}

/**
 * @brief  Queue a link lost/restored event for a device.
 * @param  id        Device index in deviceBuff.
 * @param  restored  1 = link restored, 0 = link lost.
 */
void RemoteHistory_Link(uint8_t id, uint8_t restored)
{
    HistoryEvent event = {0};

    if (id >= MAXIMUM_DEVICE || storageData.devices[id].deleted)
    {
        return;
    }

    event.serial = storageData.devices[id].factoryInfo.serial;
    event.timestamp = systemInfo.currentTime;
    event.action = restored ? HIST_LINK_RESTORED : HIST_LINK_LOST;
    event.source = HIST_MONITOR;
    /* Link detection is not motor feedback: never present stale ON/OFF as a fact. */
    event.before = event.after = eMotor_PowerState_Idle;
    event.result = restored ? HIST_APPLIED : HIST_WARNING;
    event.error = restored ? 0 : HIST_ERROR_NO_STATUS;
    event.flags = HIST_LOCAL;
    if (event.timestamp < 1704067200u)
    {
        event.flags |= HIST_TIME_UNKNOWN;
    }
    enqueue(&event, RemoteHistory_Owner(id), 0);
}

/**
 * @brief  Whether events are waiting to be stored.
 * @return 1 while busy; the remote does not sleep then.
 */
uint8_t RemoteHistory_Busy(void)
{
    for (uint8_t i = 0; i < PENDING_SLOTS; i++)
    {
        if (pending[i].ready)
        {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief  Store the queued events and acknowledge the device events.
 * @note   Call from systemTask; writes flash.
 */
void RemoteHistory_Tick(void)
{
    RemoteHistory_Pending_t *p;
    uint8_t existing;
    HistoryAck ack;
    LoraPackage_t packet;

    for (uint8_t i = 0; i < PENDING_SLOTS; i++)
    {
        if (pending[i].ready != 1)
        {
            continue;
        }
        pending[i].ready = 2;
        p = &pending[i];

        /* Store the event unless it is already in the journal. */
        existing = p->event.id ? HJ_Contains(&p->event, p->owner) : 0;
        if (existing == 2 || (!existing && !HJ_Append(&p->event, p->owner)))
        {
            remoteHistoryRejected++;
            p->ready = 0;
            continue;
        }
        if (existing)
        {
            remoteHistoryDuplicates++;
        }
        else
        {
            remoteHistoryStored++;
            remoteHistoryRevision++;
        }

        /* Acknowledge slave events with the original transaction ID. */
        if (!(p->event.flags & HIST_LOCAL))
        {
            ack.serial = p->event.serial;
            ack.id = p->event.id;
            ack.eventCrc = HJ_Crc(&p->event, sizeof(p->event));
            packet = Comm_CreateNewPackage(p->owner >> 8, p->owner & 255, CMD_HISTORY,
                LORA_PACKAGE_TYPE_RESPONSE, (uint8_t *)&ack, sizeof(ack), 0);
            packet.transactionID = p->transaction;
            memcpy(packet.rawData + 6, &packet.transactionID, 2);
            packet.checkSum = Util_CalculateCRC(packet.rawData, packet.length);
            packet.rawData[packet.byteCount - 1] = packet.checkSum;
            if (osMessageQueuePut(txMsgQueueHandle, &packet, 0, 0) == osOK)
            {
                remoteHistoryAcked++;
            }
        }
        p->ready = 0;
    }
}

static void enqueue(const HistoryEvent *event, uint16_t owner, uint16_t transaction)
{
    taskENTER_CRITICAL();
    for (uint8_t i = 0; i < PENDING_SLOTS; i++)
    {
        if (!pending[i].ready)
        {
            pending[i].event = *event;
            pending[i].owner = owner;
            pending[i].transaction = transaction;
            pending[i].ready = 1;
            taskEXIT_CRITICAL();
            return;
        }
    }
    taskEXIT_CRITICAL();
    remoteHistoryRejected++;
}
