#include "link_monitor.h"
#include "main.h"
#include "screens.h"
#include "utility.h"
#include "remote_history.h"
#include "cmsis_os.h"
#include <string.h>
#include <stddef.h>

#define LINK_MAGIC                 0x314B4E4Cu
#define LINK_CONFIRM_ATTEMPTS      3
#define LINK_REPLY_TIMEOUT_SECONDS 10

extern SystemInfo_t systemInfo;
extern SystemStorageData_t storageData;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];
extern osMessageQueueId_t txMsgQueueHandle;

Link_DeviceState_t linkDevices[MAXIMUM_DEVICE];
Link_ProbeState_t linkProbe;

static uint32_t uptimeSeconds;
static uint32_t previousRtc;
static uint32_t lastScan;
static uint32_t probeGeneration;
static uint8_t scanRequested;
static uint8_t nextDevice;
static uint32_t syncPending;
static uint8_t syncReply;
static volatile uint8_t syncRequested;

static uint8_t extensionChecksum(void);
static void saveSettings(void);
static void beginStatusSync(void);
static void endProbe(uint8_t failed);
static void sendProbe(void);

/* Settings ================================================================= */

uint16_t Link_GetIdleSeconds(void)
{
    return storageData.sleepAfterIdleSeconds;
}

uint16_t Link_GetTimeoutSeconds(void)
{
    return storageData.connectionTimeoutSeconds;
}

uint16_t Link_GetAutoStatus(uint8_t id)
{
    return id < MAXIMUM_DEVICE ? storageData.autoStatusSeconds[id] : 0;
}

void Link_SetAutoStatus(uint8_t id, uint16_t seconds)
{
    if (id < MAXIMUM_DEVICE && seconds <= 9999)
    {
        storageData.autoStatusSeconds[id] = seconds;
        saveSettings();
    }
}

void Link_SetIdleSeconds(uint16_t seconds)
{
    if (seconds && seconds <= 9999)
    {
        storageData.sleepAfterIdleSeconds = seconds;
        saveSettings();
    }
}

void Link_SetTimeoutSeconds(uint16_t seconds)
{
    if (seconds && seconds <= 9999)
    {
        storageData.connectionTimeoutSeconds = seconds;
        saveSettings();
    }
}

/* Initialisation and clock ================================================= */

void Link_Init(uint32_t rtcSecond)
{
    uint8_t valid = storageData.linkSettingsMagic == LINK_MAGIC &&
        storageData.linkSettingsChecksum == extensionChecksum() &&
        storageData.sleepAfterIdleSeconds && storageData.sleepAfterIdleSeconds <= 9999 &&
        storageData.connectionTimeoutSeconds && storageData.connectionTimeoutSeconds <= 9999;

    /* Validate stored settings; fall back to defaults when corrupt. */
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        if (storageData.autoStatusSeconds[id] > 9999)
        {
            valid = 0;
        }
    }
    if (!valid)
    {
        storageData.sleepAfterIdleSeconds = 60;
        storageData.connectionTimeoutSeconds = 180;
        for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
        {
            storageData.autoStatusSeconds[id] = 60;
        }
        saveSettings();
    }

    /* Reset runtime state. */
    memset(linkDevices, 0, sizeof(linkDevices));
    memset(&linkProbe, 0, sizeof(linkProbe));
    linkProbe.device = -1;
    previousRtc = rtcSecond % 86400u;
    uptimeSeconds = lastScan = 0;
    scanRequested = nextDevice = 0;
    syncPending = 0;
    syncReply = 0;
    syncRequested = 0;
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        linkDevices[id].node = deviceBuff[id].nodeId;
    }
}

/* RTC seconds-of-day survives STOP; no RTOS tick or adjustable wall date needed. */
void Link_ClockStep(uint32_t rtcSecond)
{
    uint32_t delta;

    rtcSecond %= 86400u;
    delta = (rtcSecond + 86400u - previousRtc) % 86400u;
    if (delta < 43200u)
    {
        uptimeSeconds += delta;
    }
    previousRtc = rtcSecond;
}

uint32_t Link_Now(void)
{
    return uptimeSeconds;
}

void Link_ClockRebase(uint32_t rtcSecond)
{
    previousRtc = rtcSecond % 86400u;
}

/* Requests ================================================================= */

void Link_RequestScan(void)
{
    scanRequested = 1;
}

uint8_t Link_IsBusy(void)
{
    return linkProbe.device >= 0 || syncPending != 0 || syncRequested;
}

/* The USB RTC setter may run in IRQ context: defer state/queue changes to Tick. */
void Link_RequestStatusSync(void)
{
    syncRequested = 1;
}

/* Received packets ========================================================= */

void Link_RecordStatusReply(uint8_t id, uint16_t transaction)
{
    if (linkProbe.device == id && linkProbe.state && transaction == linkProbe.transaction)
    {
        syncReply = 1;
    }
}

void Link_RecordPacket(uint8_t id, uint8_t status)
{
    if (id >= MAXIMUM_DEVICE || deviceBuff[id].isDelete)
    {
        return;
    }
    linkDevices[id].lastRx = uptimeSeconds;
    if (status)
    {
        if (deviceBuff[id].isConnectionError)
        {
            RemoteHistory_Link(id, 1);
        }
        linkDevices[id].lastStatus = uptimeSeconds;
        linkDevices[id].statusGeneration++;
        deviceBuff[id].isConnectionError = 0;
        linkProbe.receivedStatus++;
    }
    scanRequested = 1; /* Check OTHER devices too, including hidden ones. */
}

void Link_RecordNodePacket(uint8_t node, uint8_t status)
{
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        if (!deviceBuff[id].isDelete && deviceBuff[id].nodeId == node)
        {
            Link_RecordPacket(id, status);
            return;
        }
    }
}

/* Transmit path ============================================================ */

uint8_t Link_ShouldTransmit(const LoraPackage_t *packet)
{
    if (packet->code != CMD_GET_STATUS || packet->type != LORA_PACKAGE_TYPE_REQUEST)
    {
        return 1;
    }
    return linkProbe.device >= 0 && packet->transactionID == linkProbe.transaction &&
        packet->nodeId == deviceBuff[linkProbe.device].nodeId && packet->netId == systemInfo.netId;
}

void Link_TxFinished(const LoraPackage_t *packet, uint8_t success)
{
    (void)success; /* Both failed TX and missing reply consume an attempt, spaced by 10 s. */
    if (packet->code == CMD_GET_STATUS && Link_ShouldTransmit(packet))
    {
        linkProbe.state = 2;
        linkProbe.deadline = HAL_GetTick() + LINK_REPLY_TIMEOUT_SECONDS * 1000u;
    }
}

/* Periodic processing ====================================================== */

void Link_Tick(void)
{
    uint8_t id;

    if (currentScreen == &devicePairingScreen)
    {
        return;
    }
    if (syncRequested)
    {
        beginStatusSync();
    }

    /* Reset link state of devices that were re-paired or deleted. */
    for (id = 0; id < MAXIMUM_DEVICE; id++)
    {
        if (linkDevices[id].node != deviceBuff[id].nodeId || deviceBuff[id].isDelete)
        {
            syncPending &= ~(1u << id);
            linkDevices[id].node = deviceBuff[id].nodeId;
            linkDevices[id].lastRx = uptimeSeconds;
            linkDevices[id].retryAt = 0;
            deviceBuff[id].isConnectionError = 0;
            if (linkProbe.device == id)
            {
                linkProbe.device = -1;
                linkProbe.state = 0;
            }
        }
    }

    /* Continue the probe in progress. */
    if (linkProbe.device >= 0)
    {
        id = linkProbe.device;
        if ((syncPending & (1u << id)) ? syncReply :
            linkDevices[id].statusGeneration != probeGeneration)
        {
            endProbe(0);
            return;
        }
        if (linkProbe.state && (int32_t)(HAL_GetTick() - linkProbe.deadline) >= 0)
        {
            if (linkProbe.attempts >= LINK_CONFIRM_ATTEMPTS)
            {
                endProbe(1);
                return;
            }
            linkProbe.state = 0;
        }
        if (!linkProbe.state)
        {
            sendProbe();
        }
        return;
    }

    /* Start a new probe for the next device that needs one. */
    if (systemInfo.runMode != eSystem_RunMode_Sleep &&
        uptimeSeconds - lastScan >= systemInfo.tUpdate)
    {
        scanRequested = 1;
    }
    if (!scanRequested && !syncPending)
    {
        return;
    }
    scanRequested = 0;
    lastScan = uptimeSeconds;
    for (uint8_t offset = 0; offset < MAXIMUM_DEVICE; offset++)
    {
        id = (nextDevice + offset) % MAXIMUM_DEVICE;
        if (!deviceBuff[id].isDelete && deviceBuff[id].nodeId && deviceBuff[id].nodeId != 255 &&
            ((syncPending & (1u << id)) ||
             (uptimeSeconds - linkDevices[id].lastRx > Link_GetTimeoutSeconds() &&
              (!linkDevices[id].retryAt || (int32_t)(uptimeSeconds - linkDevices[id].retryAt) >= 0))))
        {
            linkProbe.device = id;
            nextDevice = (id + 1) % MAXIMUM_DEVICE;
            linkProbe.attempts = linkProbe.state = 0;
            syncReply = 0;
            probeGeneration = linkDevices[id].statusGeneration;
            sendProbe();
            return;
        }
    }
}

/* Private helpers ========================================================== */

static uint8_t extensionChecksum(void)
{
    return Util_CalculateCRC((uint8_t *)&storageData.linkSettingsMagic,
        offsetof(SystemStorageData_t, linkSettingsChecksum) -
        offsetof(SystemStorageData_t, linkSettingsMagic));
}

static void saveSettings(void)
{
    storageData.linkSettingsMagic = LINK_MAGIC;
    storageData.linkSettingsChecksum = extensionChecksum();
    systemInfo.isNeedSaveConfig = 1;
}

static void beginStatusSync(void)
{
    syncRequested = 0;
    /* One unicast per configured device, including hidden/locked devices.
       Cancel stale queued probes; Link_ShouldTransmit filters their transaction. */
    linkProbe.device = -1;
    linkProbe.state = 0;
    syncPending = 0;
    syncReply = 0;
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        if (!deviceBuff[id].isDelete && deviceBuff[id].nodeId && deviceBuff[id].nodeId != 255)
        {
            syncPending |= 1u << id;
        }
    }
    scanRequested = 1;
}

static void endProbe(uint8_t failed)
{
    uint8_t id = linkProbe.device;
    syncPending &= ~(1u << id);
    linkProbe.device = -1;
    linkProbe.state = 0;
    scanRequested = 1;
    if (failed)
    {
        if (!deviceBuff[id].isConnectionError)
        {
            RemoteHistory_Link(id, 0);
        }
        deviceBuff[id].isConnectionError = 1;
        linkDevices[id].retryAt = uptimeSeconds + Link_GetTimeoutSeconds();
        linkProbe.failures++;
        ScrnDcs_ShowConnectionError(id);
    }
    else
    {
        linkProbe.confirmations++;
    }
}

static void sendProbe(void)
{
    uint8_t id = linkProbe.device;
    PayloadRequestGetStatus_t payload;
    LoraPackage_t packet;

    payload.moduleIndex = 0;
    /* Never overwrite a slave's valid clock with the default January 2000 RTC. */
    payload.syncTime = systemInfo.currentTime >= 1704067200u ? systemInfo.currentTime : 0;
    packet = Comm_CreateReqGetStatus(systemInfo.netId, deviceBuff[id].nodeId, payload);
    /* Publish before queueing: the higher-priority sender can run immediately. */
    linkProbe.transaction = packet.transactionID;
    linkProbe.state = 1;
    linkProbe.deadline = HAL_GetTick() + LINK_REPLY_TIMEOUT_SECONDS * 1000u;
    if (osMessageQueuePut(txMsgQueueHandle, &packet, 0, 0) == osOK)
    {
        linkProbe.attempts++;
        linkProbe.requests++;
    }
    else
    {
        linkProbe.state = 0;
    }
}
