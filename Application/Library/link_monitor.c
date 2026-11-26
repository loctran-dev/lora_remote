#include "link_monitor.h"
#include "main.h"
#include "config_store.h"
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
extern DeviceInfo_t devicesInfo[MAXIMUM_DEVICE];
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

/**
 * @brief  Idle time before the remote enters SLEEP.
 * @return 1 .. 9999 [s].
 */
uint16_t Link_GetIdleSeconds(void)
{
    return storageData.idleTimeout;
}

/**
 * @brief  Silence after which a device is probed, and the back-off after a failed probe.
 * @return 1 .. 9999 [s].
 */
uint16_t Link_GetTimeoutSeconds(void)
{
    return storageData.linkTimeout;
}

/**
 * @brief  Auto-status period of a device, sent to it in SET_CONFIG.
 * @param  id  Device index in devicesInfo.
 * @return 0 .. 9999 [s], or 0 for an invalid id.
 */
uint16_t Link_GetAutoStatus(uint8_t id)
{
    return id < MAXIMUM_DEVICE ? storageData.autoStatusPeriod[id] : 0;
}

/**
 * @brief  Set the auto-status period of a device and mark the settings for saving.
 * @param  id       Device index in devicesInfo; ignored if out of range.
 * @param  seconds  0 .. 9999 [s]; larger values are ignored.
 */
void Link_SetAutoStatus(uint8_t id, uint16_t seconds)
{
    if (id < MAXIMUM_DEVICE && seconds <= 9999)
    {
        storageData.autoStatusPeriod[id] = seconds;
        saveSettings();
    }
}

/**
 * @brief  Set the idle time before SLEEP and mark the settings for saving.
 * @param  seconds  1 .. 9999 [s]; other values are ignored.
 */
void Link_SetIdleSeconds(uint16_t seconds)
{
    if (seconds && seconds <= 9999)
    {
        storageData.idleTimeout = seconds;
        saveSettings();
    }
}

/**
 * @brief  Set the connection timeout and mark the settings for saving.
 * @param  seconds  1 .. 9999 [s]; other values are ignored.
 */
void Link_SetTimeoutSeconds(uint16_t seconds)
{
    if (seconds && seconds <= 9999)
    {
        storageData.linkTimeout = seconds;
        saveSettings();
    }
}

/**
 * @brief  Validate the stored link settings and reset the runtime state.
 *
 * Restores the defaults (idle 60 s, timeout 180 s, auto-status 60 s) when
 * the magic, checksum or a value is invalid, and starts the uptime at 0.
 *
 * @param  rtcSecond  Current RTC value, from Power_ReadRtcSecond().
 * @note   Call once from main() after devicesInfo is loaded.
 */
void Link_Init(uint32_t rtcSecond)
{
    uint8_t valid = storageData.linkMagic == LINK_MAGIC &&
        storageData.linkCrc == extensionChecksum() &&
        storageData.idleTimeout && storageData.idleTimeout <= 9999 &&
        storageData.linkTimeout && storageData.linkTimeout <= 9999;

    /* Validate stored settings; fall back to defaults when corrupt. */
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        if (storageData.autoStatusPeriod[id] > 9999)
        {
            valid = 0;
        }
    }
    if (!valid)
    {
        storageData.idleTimeout = 60;
        storageData.linkTimeout = 180;
        for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
        {
            storageData.autoStatusPeriod[id] = 60;
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
        linkDevices[id].node = storageData.devices[id].nodeId;
    }
}

/**
 * @brief  Advance the uptime by the RTC seconds elapsed since the last call.
 *
 * Steps of 12 h or more are treated as a clock change and ignored.
 *
 * @param  rtcSecond  Current RTC value, from Power_ReadRtcSecond().
 * @note   Called every systemTask loop and right after waking from STOP.
 */
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

/**
 * @brief  Uptime from the RTC; unlike the RTOS tick it keeps counting in STOP.
 * @return Seconds since Link_Init().
 */
uint32_t Link_Now(void)
{
    return uptimeSeconds;
}

/**
 * @brief  Take a new RTC value as reference without advancing the uptime.
 * @param  rtcSecond  RTC value just written, from Power_ReadRtcSecond().
 * @note   Call after the RTC time is set.
 */
void Link_ClockRebase(uint32_t rtcSecond)
{
    previousRtc = rtcSecond % 86400u;
}

/** @brief Ask the next Link_Tick() to look for a device that needs a probe. */
void Link_RequestScan(void)
{
    scanRequested = 1;
}

/**
 * @brief  Whether a probe or status sync is in progress.
 * @return 1 while busy; Power_TryStop() does not enter STOP then.
 */
uint8_t Link_IsBusy(void)
{
    return linkProbe.device >= 0 || syncPending != 0 || syncRequested;
}

/**
 * @brief  Ask the next Link_Tick() to probe every paired device once.
 * @note   Safe from IRQ context (USB RTC setter); the work is done in Link_Tick().
 */
void Link_RequestStatusSync(void)
{
    syncRequested = 1;
}

/**
 * @brief  Mark the status sync reply of the device being probed as received.
 * @param  id           Device index in devicesInfo.
 * @param  transaction  Transaction id of the reply; must match the probe.
 */
void Link_RecordStatusReply(uint8_t id, uint16_t transaction)
{
    if (linkProbe.device == id && linkProbe.state && transaction == linkProbe.transaction)
    {
        syncReply = 1;
    }
}

/**
 * @brief  Record a packet from a device and clear its connection error.
 * @param  id      Device index in devicesInfo; deleted devices are ignored.
 * @param  status  1 if the packet carried device status, 0 otherwise.
 * @note   Requests a scan so other silent devices are checked too.
 */
void Link_RecordPacket(uint8_t id, uint8_t status)
{
    if (id >= MAXIMUM_DEVICE || storageData.devices[id].deleted)
    {
        return;
    }
    linkDevices[id].lastRx = uptimeSeconds;
    if (status)
    {
        if (devicesInfo[id].linkLost)
        {
            RemoteHistory_Link(id, 1);
        }
        linkDevices[id].lastStatus = uptimeSeconds;
        linkDevices[id].statusGeneration++;
        devicesInfo[id].linkLost = 0;
        linkProbe.receivedStatus++;
    }
    scanRequested = 1; /* Check OTHER devices too, including hidden ones. */
}

/**
 * @brief  Link_RecordPacket() for the paired device with the given node id.
 * @param  node    LoRa node id of the sender.
 * @param  status  1 if the packet carried device status, 0 otherwise.
 */
void Link_RecordNodePacket(uint8_t node, uint8_t status)
{
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        if (!storageData.devices[id].deleted && storageData.devices[id].nodeId == node)
        {
            Link_RecordPacket(id, status);
            return;
        }
    }
}

/**
 * @brief  Filter stale GET_STATUS requests before they are sent.
 * @param  packet  Packet taken from txMsgQueue.
 * @return 1 to send it: every packet except a GET_STATUS request that does
 *         not belong to the probe in progress.
 * @note   Called by loraSendTask.
 */
uint8_t Link_ShouldTransmit(const LoraPackage_t *packet)
{
    if (packet->code != CMD_GET_STATUS || packet->type != LORA_PACKAGE_TYPE_REQUEST)
    {
        return 1;
    }
    return linkProbe.device >= 0 && packet->transactionID == linkProbe.transaction &&
        packet->nodeId == storageData.devices[linkProbe.device].nodeId && packet->netId == systemInfo.netId;
}

/**
 * @brief  Start the reply timeout once the probe request has been sent.
 * @param  packet   Packet just transmitted.
 * @param  success  Radio result; unused, a failed send and a missing reply
 *                  both use up an attempt.
 * @note   Called by loraSendTask.
 */
void Link_TxFinished(const LoraPackage_t *packet, uint8_t success)
{
    (void)success; /* Both failed TX and missing reply consume an attempt, spaced by 10 s. */
    if (packet->code == CMD_GET_STATUS && Link_ShouldTransmit(packet))
    {
        linkProbe.state = 2;
        linkProbe.deadline = HAL_GetTick() + LINK_REPLY_TIMEOUT_SECONDS * 1000u;
    }
}

/**
 * @brief  Run the link supervision: continue the probe in progress or start the next one.
 *
 * Resets devices that were re-paired or deleted, retries a probe whose reply
 * timed out (up to 3 attempts) and marks the device as a connection error
 * when all attempts fail. Outside SLEEP a scan starts every scanPeriod seconds.
 * Does nothing while the pairing screen is open.
 *
 * @note   Call from systemTask with uiMutex held; skipped while a LoRa packet
 *         is pending.
 */
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
        if (linkDevices[id].node != storageData.devices[id].nodeId || storageData.devices[id].deleted)
        {
            syncPending &= ~(1u << id);
            linkDevices[id].node = storageData.devices[id].nodeId;
            linkDevices[id].lastRx = uptimeSeconds;
            linkDevices[id].retryAt = 0;
            devicesInfo[id].linkLost = 0;
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
        uptimeSeconds - lastScan >= storageData.scanPeriod)
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
        if (!storageData.devices[id].deleted && storageData.devices[id].nodeId && storageData.devices[id].nodeId != 255 &&
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

static uint8_t extensionChecksum(void)
{
    return Util_CalculateCRC((uint8_t *)&storageData.linkMagic,
        offsetof(SystemStorageData_t, linkCrc) -
        offsetof(SystemStorageData_t, linkMagic));
}

static void saveSettings(void)
{
    storageData.linkMagic = LINK_MAGIC;
    storageData.linkCrc = extensionChecksum();
    systemInfo.configSavePending = 1;
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
        if (!storageData.devices[id].deleted && storageData.devices[id].nodeId && storageData.devices[id].nodeId != 255)
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
        if (!devicesInfo[id].linkLost)
        {
            RemoteHistory_Link(id, 0);
        }
        devicesInfo[id].linkLost = 1;
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

    payload.moduleIndex = LORA_MODULE_ALL; /* one reply refreshes every motor of the device */
    /* Never overwrite a slave's valid clock with the default January 2000 RTC. */
    payload.syncTime = systemInfo.currentTime >= 1704067200u ? systemInfo.currentTime : 0;
    packet = Comm_CreateReqGetStatus(systemInfo.netId, storageData.devices[id].nodeId, payload);
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
