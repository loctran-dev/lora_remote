#include "command_handlers.h"
#include "communication_lora.h"
#include "string.h"
#include "utility.h"
#include "main.h"
#include "cmsis_os.h"
#include "define.h"
#include "config_store.h"
#include "lora.h"
#include "link_monitor.h"
#include "power_sleep.h"
#include "remote_history.h"
#include "schedule.h"

extern osMessageQueueId_t txMsgQueueHandle;
extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];
extern LoRa sLoRaCtrl;

static uint16_t configTransaction;
static uint8_t configNode;

/* Private handlers are dispatched only through cmdTable. */
static void handleHistory(LoraPackage_t packet);
static void handleSetConfig(LoraPackage_t packet);
static void handleGetStatus(LoraPackage_t packet);
static void handleControlPower(LoraPackage_t packet);
static void handleSetSchedule(LoraPackage_t packet);
static void handleSyncTime(LoraPackage_t packet);
static void handleGetFactoryInfo(LoraPackage_t packet);
static void handleConfigConnection(LoraPackage_t packet);
static void handleAlarmMessage(LoraPackage_t packet);

/* Command dispatch table. Keep this order aligned with implementations below. */
const CmdMapping_t cmdTable[] = {
    {CMD_HISTORY, handleHistory},
    {CMD_SET_CONFIG, handleSetConfig},
    {CMD_GET_STATUS, handleGetStatus},
    {CMD_CONTROL_POWER, handleControlPower},
    {CMD_SET_SCHEDULE, handleSetSchedule},
    {CMD_SYNC_TIME, handleSyncTime},
    {CMD_GET_FACTORY_INFO, handleGetFactoryInfo},
    {CMD_CONFIG_CONNECTION, handleConfigConnection},
    {CMD_ALARM_MESSAGE, handleAlarmMessage},
    {0, NULL}
};

/**
 * @brief  Queue a CMD_SET_CONFIG request with the settings of the selected module.
 *
 * Sends the alarm threshold and delay of the current device/module and its
 * auto-status period. The reply gives two short beeps if accepted, a long
 * beep if rejected.
 *
 * @return TRUE if the request was queued; FALSE for a deleted device, a value
 *         out of range or a full queue.
 */
uint8_t CmdHandler_SendCurrentModuleConfig(void)
{
    DeviceStorageData_t *device = &storageData.DeviceConfig[systemInfo.currentDeviceId];
    PayloadRequestSetConfig_t config;
    LoraPackage_t packet;
    if (device->isDelete || !(device->alarmThreshold >= 0 && device->alarmThreshold <= 99.99f) || device->alarmDelayTime > 9999)
    {
        return FALSE;
    }
    config.moduleIndex = systemInfo.currentModuleId;
    config.alarmThresholdCentiAmp = (uint16_t)(device->alarmThreshold * 100.0f + 0.5f);
    config.alarmDelaySeconds = device->alarmDelayTime;
    config.autoStatusSeconds = Link_GetAutoStatus(systemInfo.currentDeviceId);
    packet = Comm_CreateNewPackage(systemInfo.netId, device->nodeIdConfig, CMD_SET_CONFIG,
                                   LORA_PACKAGE_TYPE_REQUEST, (uint8_t *)&config, sizeof(config), 0);
    configTransaction = packet.transactionID;
    configNode = device->nodeIdConfig;
    return osMessageQueuePut(txMsgQueueHandle, &packet, 0, 0) == osOK;
}

static void handleHistory(LoraPackage_t packet)
{
    if (packet.code != CMD_HISTORY)
    {
        return;
    }
    RemoteHistory_Handle(packet);
}

static void handleSetConfig(LoraPackage_t packet)
{
    PayloadCommonResponse_t reply;
    if (packet.type != LORA_PACKAGE_TYPE_RESPONSE ||
        packet.netId != systemInfo.netId || packet.nodeId != configNode ||
        packet.transactionID != configTransaction ||
        packet.byteCount != LORA_PACKAGE_HEADER_SIZE + sizeof(reply) + LORA_PACKAGE_CRC_SIZE)
    {
        return;
    }
    memcpy(&reply, &packet.rawData[LORA_PACKAGE_HEADER_SIZE], sizeof(reply));
    configTransaction = 0;
    Link_RecordNodePacket(packet.nodeId, 0);
    Main_SetBuzzerMode(reply.result ? eBuzzer_Mode_2ShortBeep : eBuzzer_Mode_LongBeep);
}

static void handleGetStatus(LoraPackage_t fwPackage)
{
    PayloadResponseGetStatus_t payloadResponse;

    if ((fwPackage.netId != systemInfo.netId) ||
        (fwPackage.nodeId == LORA_NODE_ID_DEFAULT || fwPackage.nodeId == LORA_NODE_ID_BROADCAST) ||
        (fwPackage.code != CMD_GET_STATUS))
    {
        return;
    }
    if (fwPackage.type != LORA_PACKAGE_TYPE_RESPONSE)
    {
        return;
    }
    if (Comm_ParsePayloadResGetStatus(fwPackage, &payloadResponse) == FALSE)
    {
        return;
    }
    if (payloadResponse.result != 1 || payloadResponse.moduleIndex != 0)
    {
        return;
    }

    for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo++)
    {
        if (storageData.DeviceConfig[devNo].isDelete == 0 && storageData.DeviceConfig[devNo].nodeIdConfig == fwPackage.nodeId)
        {
            Link_RecordPacket(devNo, 1);
            Link_RecordStatusReply(devNo, fwPackage.transactionID);
            deviceBuff[devNo].motorData[0].pwrState = (Motor_PowerState_e)payloadResponse.powerState;
            deviceBuff[devNo].localTime = payloadResponse.localTime;
            deviceBuff[devNo].motorData[0].scheduleTime = payloadResponse.scheduleTime;
            Schedule_HandleStatus(&fwPackage, &payloadResponse);
            deviceBuff[devNo].motorData[0].phaseCurrent[0] = (float)payloadResponse.phaseCurrent[0] * 0.01f;
            deviceBuff[devNo].motorData[0].phaseCurrent[1] = (float)payloadResponse.phaseCurrent[1] * 0.01f;
            deviceBuff[devNo].motorData[0].phaseCurrent[2] = (float)payloadResponse.phaseCurrent[2] * 0.01f;
            deviceBuff[devNo].lastCommTick = xTaskGetTickCount();
            deviceBuff[devNo].lastCommTime = systemInfo.currentTime;
            deviceBuff[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
        }
    }
}

static void handleControlPower(LoraPackage_t fwPackage)
{
    PayloadCommonResponse_t payloadResponse;

    if ((fwPackage.netId != systemInfo.netId) ||
        (fwPackage.nodeId == LORA_NODE_ID_DEFAULT || fwPackage.nodeId == LORA_NODE_ID_BROADCAST) ||
        (fwPackage.code != CMD_CONTROL_POWER))
    {
        return;
    }
    if (fwPackage.type != LORA_PACKAGE_TYPE_RESPONSE ||
        fwPackage.byteCount != LORA_PACKAGE_HEADER_SIZE + sizeof(payloadResponse) + LORA_PACKAGE_CRC_SIZE)
    {
        return;
    }
    memcpy(&payloadResponse, &fwPackage.rawData[LORA_PACKAGE_HEADER_SIZE], sizeof(payloadResponse));

    for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo++)
    {
        if (storageData.DeviceConfig[devNo].isDelete == 0 && storageData.DeviceConfig[devNo].nodeIdConfig == fwPackage.nodeId)
        {
            Link_RecordPacket(devNo, 0);
            deviceBuff[devNo].lastCommTick = xTaskGetTickCount();
            deviceBuff[devNo].lastCommTime = systemInfo.currentTime;
            deviceBuff[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
        }
    }
}

static void handleSetSchedule(LoraPackage_t fwPackage)
{
    PayloadCommonResponse_t payloadResponse;
    uint32_t deadline;
    uint8_t accepted;

    if ((fwPackage.netId != systemInfo.netId) ||
        (fwPackage.nodeId == LORA_NODE_ID_DEFAULT || fwPackage.nodeId == LORA_NODE_ID_BROADCAST) ||
        (fwPackage.code != CMD_SET_SCHEDULE))
    {
        return;
    }
    if (fwPackage.type != LORA_PACKAGE_TYPE_RESPONSE ||
        fwPackage.byteCount != LORA_PACKAGE_HEADER_SIZE + sizeof(payloadResponse) + LORA_PACKAGE_CRC_SIZE)
    {
        return;
    }
    memcpy(&payloadResponse, &fwPackage.rawData[LORA_PACKAGE_HEADER_SIZE], sizeof(payloadResponse));
    if (!Schedule_HandleResponse(&fwPackage, &payloadResponse, &deadline))
    {
        return;
    }
    accepted = payloadResponse.result == 1;
    Main_SetBuzzerMode(accepted ? eBuzzer_Mode_2ShortBeep : eBuzzer_Mode_LongBeep);

    for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo++)
    {
        if (storageData.DeviceConfig[devNo].isDelete == 0 && storageData.DeviceConfig[devNo].nodeIdConfig == fwPackage.nodeId)
        {
            /* ACK confirms this exact deadline; STATUS remains authoritative afterwards. */
            if (accepted && payloadResponse.moduleIndex < sizeof(deviceBuff[devNo].motorData) / sizeof(deviceBuff[devNo].motorData[0]))
            {
                deviceBuff[devNo].motorData[payloadResponse.moduleIndex].scheduleTime = deadline;
            }
            Link_RecordPacket(devNo, 0);
            deviceBuff[devNo].lastCommTick = xTaskGetTickCount();
            deviceBuff[devNo].lastCommTime = systemInfo.currentTime;
            deviceBuff[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
        }
    }
}

static void handleSyncTime(LoraPackage_t fwPackage)
{
    PayloadCommonResponse_t payloadResponse;

    if ((fwPackage.netId != systemInfo.netId) ||
        (fwPackage.nodeId == LORA_NODE_ID_DEFAULT || fwPackage.nodeId == LORA_NODE_ID_BROADCAST) ||
        (fwPackage.code != CMD_SYNC_TIME))
    {
        return;
    }
    if (fwPackage.type != LORA_PACKAGE_TYPE_RESPONSE ||
        fwPackage.byteCount != LORA_PACKAGE_HEADER_SIZE + sizeof(payloadResponse) + LORA_PACKAGE_CRC_SIZE)
    {
        return;
    }
    memcpy(&payloadResponse, &fwPackage.rawData[LORA_PACKAGE_HEADER_SIZE], sizeof(payloadResponse));

    for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo++)
    {
        if (storageData.DeviceConfig[devNo].isDelete == 0 && storageData.DeviceConfig[devNo].nodeIdConfig == fwPackage.nodeId)
        {
            Link_RecordPacket(devNo, 0);
            deviceBuff[devNo].lastCommTick = xTaskGetTickCount();
            deviceBuff[devNo].lastCommTime = systemInfo.currentTime;
            deviceBuff[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
        }
    }
}

static void handleGetFactoryInfo(LoraPackage_t fwPackage)
{
    PayloadResponseGetFactoryInfo_t payloadResponse;

    if ((fwPackage.netId != systemInfo.netId) ||
        (fwPackage.nodeId != LORA_NODE_ID_DEFAULT && fwPackage.nodeId != LORA_NODE_ID_BROADCAST) ||
        (fwPackage.code != CMD_GET_FACTORY_INFO))
    {
        return;
    }
    if (Comm_ParsePayloadResGetFactoryInfo(fwPackage, &payloadResponse) == FALSE)
    {
        return;
    }

    if (systemInfo.findDeviceStep == eProcess_Step_Processing)
    {
        systemInfo.findDeviceStep = eProcess_Step_Done;

        if (payloadResponse.result)
        {
            systemInfo.findDeviceResult = eProcess_Result_Passed;
            systemInfo.findDevicefactoryInfo.serial = payloadResponse.serial;
            systemInfo.findDevicefactoryInfo.deviceType = payloadResponse.deviceType;
            systemInfo.findDevicefactoryInfo.numberOfModule = payloadResponse.numberOfModule;
        }
        else
        {
            systemInfo.findDeviceResult = eProcess_Result_Failed;
        }
    }
}

static void handleConfigConnection(LoraPackage_t fwPackage)
{
    PayloadResponseConfigConnection_t payloadResponse;

    if ((fwPackage.netId != systemInfo.netId) ||
        (fwPackage.nodeId != LORA_NODE_ID_DEFAULT && fwPackage.nodeId != LORA_NODE_ID_BROADCAST) ||
        (fwPackage.code != CMD_CONFIG_CONNECTION))
    {
        return;
    }
    if (Comm_ParsePayloadResConfigConnection(fwPackage, &payloadResponse) == FALSE)
    {
        return;
    }

    if (systemInfo.configDeviceStep == eProcess_Step_Processing)
    {
        systemInfo.configDeviceStep = eProcess_Step_Done;
        /* A reply from another serial leaves configDeviceResult unchanged. */
        if (payloadResponse.serial == systemInfo.findDevicefactoryInfo.serial)
        {
            if (payloadResponse.result == TRUE)
            {
                systemInfo.configDeviceResult = eProcess_Result_Passed;
            }
            else
            {
                systemInfo.configDeviceResult = eProcess_Result_Failed;
            }
        }
    }
}

static void handleAlarmMessage(LoraPackage_t fwPackage)
{
    PayloadRequestAlarm_t payloadRequest;

    if ((fwPackage.netId != systemInfo.netId) ||
        (fwPackage.nodeId == LORA_NODE_ID_DEFAULT || fwPackage.nodeId == LORA_NODE_ID_BROADCAST) ||
        (fwPackage.code != CMD_ALARM_MESSAGE))
    {
        return;
    }
    if (Comm_ParsePayloadReqAlarm(fwPackage, &payloadRequest) == FALSE)
    {
        return;
    }
    if (fwPackage.type != LORA_PACKAGE_TYPE_REQUEST || payloadRequest.moduleIndex != 0)
    {
        return;
    }
    Link_RecordNodePacket(fwPackage.nodeId, 0);

    for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo++)
    {
        if (storageData.DeviceConfig[devNo].isDelete == 0 && storageData.DeviceConfig[devNo].nodeIdConfig == fwPackage.nodeId)
        {
            deviceBuff[devNo].motorData[0].isAlarmOccurred = TRUE;
            Power_WakeForError();
            deviceBuff[devNo].motorData[0].alarm = (Alarm_Name_e)payloadRequest.alarmCode;
            deviceBuff[devNo].motorData[0].alarmTime = payloadRequest.alarmTime;
            deviceBuff[devNo].lastCommTick = xTaskGetTickCount();
            deviceBuff[devNo].lastCommTime = systemInfo.currentTime;
            deviceBuff[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
        }
    }
}
