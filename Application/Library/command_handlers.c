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
extern DeviceInfo_t devicesInfo[MAXIMUM_DEVICE];
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
static void applyMotorStatus(uint8_t devNo, const LoraPackage_t *packet, const PayloadStatusRecord_t *record,
    uint32_t localTime);

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
    DeviceStorageData_t *device = &storageData.devices[systemInfo.curDevice];
    MotorStorageData_t *motor = Config_Motor(systemInfo.curDevice, systemInfo.curModule);
    PayloadRequestSetConfig_t config;
    LoraPackage_t packet;
    if (device->deleted || motor == NULL ||
        !(motor->alarmThreshold >= 0 && motor->alarmThreshold <= 99.99f) || motor->alarmDelay > 9999)
    {
        return FALSE;
    }
    config.moduleIndex = systemInfo.curModule;
    config.alarmThresholdCentiAmp = (uint16_t)(motor->alarmThreshold * 100.0f + 0.5f);
    config.alarmDelaySeconds = motor->alarmDelay;
    config.autoStatusSeconds = Link_GetAutoStatus(systemInfo.curDevice);
    packet = Comm_CreateNewPackage(systemInfo.netId, device->nodeId, CMD_SET_CONFIG,
                                   LORA_PACKAGE_TYPE_REQUEST, (uint8_t *)&config, sizeof(config), 0);
    configTransaction = packet.transactionID;
    configNode = device->nodeId;
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

/*
 * Status of one motor (moduleIndex 0..5) or of every motor of the device
 * (moduleIndex LORA_MODULE_ALL, also used by autonomous reports). Both forms
 * are brought to the multi-record layout, then each valid record updates the
 * state and tracked schedule of its own module.
 */
static void handleGetStatus(LoraPackage_t fwPackage)
{
    PayloadResponseGetStatusAll_t status;
    PayloadResponseGetStatus_t single;
    uint8_t valid = 0;

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
    if (Comm_ParsePayloadResGetStatusAll(fwPackage, &status) == FALSE)
    {
        if (Comm_ParsePayloadResGetStatus(fwPackage, &single) == FALSE)
        {
            return;
        }
        status.localTime = single.localTime;
        status.count = 1;
        status.records[0].moduleIndex = single.moduleIndex;
        status.records[0].result = single.result;
        status.records[0].powerState = single.powerState;
        memcpy(status.records[0].phaseCurrent, single.phaseCurrent, sizeof(single.phaseCurrent));
        status.records[0].scheduleTime = single.scheduleTime;
    }
    for (uint8_t i = 0; i < status.count; i++)
    {
        valid |= status.records[i].result == 1 && status.records[i].moduleIndex < MAXIMUM_MODULE;
    }
    if (!valid)
    {
        return;
    }

    for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo++)
    {
        if (storageData.devices[devNo].deleted == 0 && storageData.devices[devNo].nodeId == fwPackage.nodeId)
        {
            Link_RecordPacket(devNo, 1);
            Link_RecordStatusReply(devNo, fwPackage.transactionID);
            devicesInfo[devNo].devTime = status.localTime;
            for (uint8_t i = 0; i < status.count; i++)
            {
                applyMotorStatus(devNo, &fwPackage, &status.records[i], status.localTime);
            }
            devicesInfo[devNo].lastRxTick = xTaskGetTickCount();
            devicesInfo[devNo].lastRxTime = systemInfo.currentTime;
            devicesInfo[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
        }
    }
}

/* One status record: confirm a matching schedule, then store it in the motor's state if it has a slot. */
static void applyMotorStatus(uint8_t devNo, const LoraPackage_t *packet, const PayloadStatusRecord_t *record,
    uint32_t localTime)
{
    PayloadResponseGetStatus_t status;
    MotorData_t *motor;

    if (record->result != 1 || record->moduleIndex >= MAXIMUM_MODULE)
    {
        return;
    }
    status.moduleIndex = record->moduleIndex;
    status.result = record->result;
    status.powerState = record->powerState;
    memcpy(status.phaseCurrent, record->phaseCurrent, sizeof(status.phaseCurrent));
    status.localTime = localTime;
    status.scheduleTime = record->scheduleTime;
    Schedule_HandleStatus(packet, &status);

    motor = Main_MotorState(devNo, record->moduleIndex);
    if (motor)
    {
        motor->pwrState = (Motor_PowerState_e)record->powerState;
        motor->autoOffTime = record->scheduleTime;
        motor->phaseCurrent[0] = (float)record->phaseCurrent[0] * 0.01f;
        motor->phaseCurrent[1] = (float)record->phaseCurrent[1] * 0.01f;
        motor->phaseCurrent[2] = (float)record->phaseCurrent[2] * 0.01f;
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
        if (storageData.devices[devNo].deleted == 0 && storageData.devices[devNo].nodeId == fwPackage.nodeId)
        {
            Link_RecordPacket(devNo, 0);
            devicesInfo[devNo].lastRxTick = xTaskGetTickCount();
            devicesInfo[devNo].lastRxTime = systemInfo.currentTime;
            devicesInfo[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
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
        if (storageData.devices[devNo].deleted == 0 && storageData.devices[devNo].nodeId == fwPackage.nodeId)
        {
            /* ACK confirms this exact deadline; STATUS remains authoritative afterwards. */
            if (accepted && Main_MotorState(devNo, payloadResponse.moduleIndex))
            {
                Main_MotorState(devNo, payloadResponse.moduleIndex)->autoOffTime = deadline;
            }
            Link_RecordPacket(devNo, 0);
            devicesInfo[devNo].lastRxTick = xTaskGetTickCount();
            devicesInfo[devNo].lastRxTime = systemInfo.currentTime;
            devicesInfo[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
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
        if (storageData.devices[devNo].deleted == 0 && storageData.devices[devNo].nodeId == fwPackage.nodeId)
        {
            Link_RecordPacket(devNo, 0);
            devicesInfo[devNo].lastRxTick = xTaskGetTickCount();
            devicesInfo[devNo].lastRxTime = systemInfo.currentTime;
            devicesInfo[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
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

    if (systemInfo.searchStep == eProcess_Step_Processing)
    {
        systemInfo.searchStep = eProcess_Step_Done;

        if (payloadResponse.result)
        {
            systemInfo.searchResult = eProcess_Result_Passed;
            systemInfo.foundDevice.serial = payloadResponse.serial;
            systemInfo.foundDevice.type = payloadResponse.deviceType;
            systemInfo.foundDevice.moduleCount = payloadResponse.numberOfModule;
        }
        else
        {
            systemInfo.searchResult = eProcess_Result_Failed;
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

    if (systemInfo.connectStep == eProcess_Step_Processing)
    {
        systemInfo.connectStep = eProcess_Step_Done;
        /* A reply from another serial leaves connectResult unchanged. */
        if (payloadResponse.serial == systemInfo.foundDevice.serial)
        {
            if (payloadResponse.result == TRUE)
            {
                systemInfo.connectResult = eProcess_Result_Passed;
            }
            else
            {
                systemInfo.connectResult = eProcess_Result_Failed;
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
    if (fwPackage.type != LORA_PACKAGE_TYPE_REQUEST || payloadRequest.moduleIndex >= MAXIMUM_MODULE)
    {
        return;
    }
    Link_RecordNodePacket(fwPackage.nodeId, 0);

    for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo++)
    {
        if (storageData.devices[devNo].deleted == 0 && storageData.devices[devNo].nodeId == fwPackage.nodeId)
        {
            MotorData_t *motor = Main_MotorState(devNo, payloadRequest.moduleIndex);

            Power_WakeForError();
            if (motor)
            {
                motor->alarmReported = TRUE;
                motor->alarmCode = (Alarm_Name_e)payloadRequest.alarmCode;
                motor->alarmDevTime = payloadRequest.alarmTime;
            }
            devicesInfo[devNo].lastRxTick = xTaskGetTickCount();
            devicesInfo[devNo].lastRxTime = systemInfo.currentTime;
            devicesInfo[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
        }
    }
}
