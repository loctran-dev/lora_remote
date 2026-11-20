#include "commandHandlers.h"
#include "communicationLora.h"
#include "string.h"
#include "utility.h"
#include "main.h"
#include "cmsis_os.h"
#include "define.h"
#include "Lora.h"
#include "linkMonitor.h"
#include "powerSleep.h"
#include "remote_history.h"
extern osMessageQueueId_t txMsgQueueHandle;
extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];
extern LoRa sLoRaCtrl;
const CmdMapping_t cmdTable[] = {
    {CMD_HISTORY, RemoteHistory_Handle},
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

static uint16_t configTransaction;
static uint8_t configNode;

volatile ScheduleRequestState_t scheduleRequest;

void Schedule_Track(const LoraPackage_t *packet, const PayloadRequestSetSchedule_t *payload)
{
    scheduleRequest.state = TIMER_IDLE;
    scheduleRequest.syncTime = payload->syncTime;
    scheduleRequest.deadline = payload->scheduleTime;
    scheduleRequest.transaction = packet->transactionID;
    scheduleRequest.net = packet->netId;
    scheduleRequest.node = packet->nodeId;
    scheduleRequest.module = payload->moduleIndex;
    scheduleRequest.attempts = 1;
    scheduleRequest.txOk = 0;
    scheduleRequest.retryAt = HAL_GetTick() + 3000u;
    scheduleRequest.state = TIMER_WAITING;
}

void Schedule_QueueFailed(void)
{
    if (scheduleRequest.state == TIMER_WAITING) scheduleRequest.state = TIMER_QUEUE_ERROR;
}

void Schedule_CancelForNode(uint8_t node)
{
    if (scheduleRequest.node == node) scheduleRequest.state = TIMER_IDLE;
}

uint8_t Schedule_StateForNode(uint8_t node)
{
    return scheduleRequest.node == node ? scheduleRequest.state : TIMER_IDLE;
}

uint8_t Schedule_ShouldTransmit(const LoraPackage_t *packet)
{
    if (packet->code != CMD_SET_SCHEDULE || packet->type != LORA_PACKAGE_TYPE_REQUEST) return 1;
    return scheduleRequest.state == TIMER_WAITING && packet->netId == scheduleRequest.net &&
        packet->nodeId == scheduleRequest.node && packet->transactionID == scheduleRequest.transaction;
}

void Schedule_TxFinished(const LoraPackage_t *packet, uint8_t success)
{
    if (Schedule_ShouldTransmit(packet) && packet->code == CMD_SET_SCHEDULE) {
        if (success) scheduleRequest.txOk++;
        scheduleRequest.retryAt = HAL_GetTick() + 3000u;
    }
}

void Schedule_Tick(void)
{
    if (scheduleRequest.state != TIMER_WAITING ||
        (int32_t)(HAL_GetTick() - scheduleRequest.retryAt) < 0) return;
    if (scheduleRequest.attempts >= 3 ||
        (scheduleRequest.deadline && systemInfo.currentTime >= scheduleRequest.deadline)) {
        scheduleRequest.state = TIMER_TIMEOUT;
        Main_SetBuzzerMode(BUZZER_LONG_BEEP);
        return;
    }
    PayloadRequestSetSchedule_t payload;
    payload.moduleIndex = scheduleRequest.module;
    payload.syncTime = scheduleRequest.syncTime;
    payload.scheduleTime = scheduleRequest.deadline;
    LoraPackage_t packet = Comm_CreateReqSetSchedule(scheduleRequest.net, scheduleRequest.node, payload);
    /* Retransmit the SAME transaction and absolute deadline: never extend a timer. */
    packet.transactionID = scheduleRequest.transaction;
    memcpy(packet.rawData + 6, &packet.transactionID, sizeof(packet.transactionID));
    packet.checkSum = calculateCRC(packet.rawData, packet.length);
    packet.rawData[packet.byteCount - 1] = packet.checkSum;
    scheduleRequest.attempts++;
    scheduleRequest.retryAt = HAL_GetTick() + 3000u;
    if (osMessageQueuePut(txMsgQueueHandle, &packet, 0, 0) != osOK) Schedule_QueueFailed();
}

uint8_t sendCurrentModuleConfig(void)
{
	DeviceInfo_t *device = &deviceBuff[systemInfo.currentDeviceId];
	MotorData_t *module = &device->motorData[systemInfo.currentModuleId];
	PayloadRequestSetConfig_t config;
	LoraPackage_t packet;
	if (device->isDelete || !(module->alarmThreshold >= 0 && module->alarmThreshold <= 99.99f) || module->alarmDelayTime > 9999)
		return FALSE;
	config.moduleIndex = systemInfo.currentModuleId;
	config.alarmThresholdCentiAmp = (uint16_t)(module->alarmThreshold * 100.0f + 0.5f);
	config.alarmDelaySeconds = module->alarmDelayTime;
	config.autoStatusSeconds = Link_GetAutoStatus(systemInfo.currentDeviceId);
	packet = Comm_CreateNewPackage(systemInfo.netId, device->nodeId, CMD_SET_CONFIG,
								   LORA_PACKAGE_TYPE_REQUEST, (uint8_t *)&config, sizeof(config), 0);
	configTransaction = packet.transactionID;
	configNode = device->nodeId;
	return osMessageQueuePut(txMsgQueueHandle, &packet, 0, 0) == osOK;
}

void handleSetConfig(LoraPackage_t packet)
{
    PayloadCommonResponse_t reply;
    if (packet.type != LORA_PACKAGE_TYPE_RESPONSE ||
        packet.netId != systemInfo.netId || packet.nodeId != configNode ||
        packet.transactionID != configTransaction ||
        packet.byteCount != LORA_PACKAGE_HEADER_SIZE + sizeof(reply) + LORA_PACKAGE_CRC_SIZE)
        return;
    memcpy(&reply, &packet.rawData[LORA_PACKAGE_HEADER_SIZE], sizeof(reply));
    configTransaction = 0;
    Link_RecordNodePacket(packet.nodeId, 0);
    Main_SetBuzzerMode(reply.result ? BUZZER_2_SHORT_BEEP : BUZZER_LONG_BEEP);
}

void handleGetStatus(LoraPackage_t fwPackage) {
	if ((fwPackage.netId != systemInfo.netId) ||
			(fwPackage.nodeId == LORA_NODE_ID_DEFAULT || fwPackage.nodeId == LORA_NODE_ID_BROADCAST) ||
			(fwPackage.code != CMD_GET_STATUS)) {
		return;
	}
	PayloadResponseGetStatus_t payloadResponse;
	if (fwPackage.type != LORA_PACKAGE_TYPE_RESPONSE) return;
	if (Comm_ParsePayloadResGetStatus(fwPackage, &payloadResponse) == FALSE) {
		return;
	}
	if (payloadResponse.result != 1 || payloadResponse.moduleIndex != 0) return;
	
	for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo ++) {
		if (deviceBuff[devNo].isDelete == 0 && deviceBuff[devNo].nodeId == fwPackage.nodeId) {
			Link_RecordPacket(devNo, 1);
            Link_RecordStatusReply(devNo, fwPackage.transactionID);
			deviceBuff[devNo].motorData[0].pwrState = (MotorPowerState_t)payloadResponse.powerState;
			deviceBuff[devNo].localTime = payloadResponse.localTime;
			deviceBuff[devNo].motorData[0].scheduleTime = payloadResponse.scheduleTime;			
			if ((scheduleRequest.state == TIMER_WAITING || scheduleRequest.state == TIMER_TIMEOUT) &&
                fwPackage.nodeId == scheduleRequest.node &&
                payloadResponse.scheduleTime == scheduleRequest.deadline &&
                (!scheduleRequest.deadline || payloadResponse.powerState == MOTOR_POWER_ON))
                scheduleRequest.state = TIMER_CONFIRMED;
			deviceBuff[devNo].motorData[0].phaseCurrent[0] = (float)payloadResponse.phaseCurrent[0] * 0.01f;
			deviceBuff[devNo].motorData[0].phaseCurrent[1] = (float)payloadResponse.phaseCurrent[1] * 0.01f;
			deviceBuff[devNo].motorData[0].phaseCurrent[2] = (float)payloadResponse.phaseCurrent[2] * 0.01f;
			deviceBuff[devNo].lastCommTick = xTaskGetTickCount();
			deviceBuff[devNo].lastCommTime = systemInfo.currentTime;
			deviceBuff[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
		}
	}
}

void handleControlPower(LoraPackage_t fwPackage) {
	if ((fwPackage.netId != systemInfo.netId) ||
			(fwPackage.nodeId == LORA_NODE_ID_DEFAULT || fwPackage.nodeId == LORA_NODE_ID_BROADCAST) ||
			(fwPackage.code != CMD_CONTROL_POWER)) {
		return;
	}
	PayloadCommonResponse_t payloadResponse;
	if (fwPackage.type != LORA_PACKAGE_TYPE_RESPONSE || fwPackage.byteCount !=
		LORA_PACKAGE_HEADER_SIZE + sizeof(payloadResponse) + LORA_PACKAGE_CRC_SIZE) return;
	memcpy(&payloadResponse, &fwPackage.rawData[LORA_PACKAGE_HEADER_SIZE], sizeof(payloadResponse));
	
	for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo ++) {
		if (deviceBuff[devNo].isDelete == 0 && deviceBuff[devNo].nodeId == fwPackage.nodeId) {
			Link_RecordPacket(devNo, 0);
			deviceBuff[devNo].lastCommTick = xTaskGetTickCount();
			deviceBuff[devNo].lastCommTime = systemInfo.currentTime;
			deviceBuff[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
		}
	}
}

void handleSetSchedule(LoraPackage_t fwPackage) {
	if ((fwPackage.netId != systemInfo.netId) ||
			(fwPackage.nodeId == LORA_NODE_ID_DEFAULT || fwPackage.nodeId == LORA_NODE_ID_BROADCAST) ||
			(fwPackage.code != CMD_SET_SCHEDULE)) {
		return;
	}
	PayloadCommonResponse_t payloadResponse;
	if (fwPackage.type != LORA_PACKAGE_TYPE_RESPONSE || fwPackage.byteCount !=
		LORA_PACKAGE_HEADER_SIZE + sizeof(payloadResponse) + LORA_PACKAGE_CRC_SIZE) return;
	memcpy(&payloadResponse, &fwPackage.rawData[LORA_PACKAGE_HEADER_SIZE], sizeof(payloadResponse));
	if ((scheduleRequest.state != TIMER_WAITING && scheduleRequest.state != TIMER_TIMEOUT) ||
        fwPackage.netId != scheduleRequest.net ||
        fwPackage.nodeId != scheduleRequest.node || fwPackage.transactionID != scheduleRequest.transaction ||
        payloadResponse.moduleIndex != scheduleRequest.module) return;
	uint8_t accepted = payloadResponse.result == 1;
	scheduleRequest.state = accepted ? TIMER_CONFIRMED : TIMER_REJECTED;
	Main_SetBuzzerMode(accepted ? BUZZER_2_SHORT_BEEP : BUZZER_LONG_BEEP);
	
	for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo ++) {
		if (deviceBuff[devNo].isDelete == 0 && deviceBuff[devNo].nodeId == fwPackage.nodeId) {
			/* ACK confirms this exact deadline; STATUS remains authoritative afterwards. */
            if (accepted && scheduleRequest.module < sizeof(deviceBuff[devNo].motorData) / sizeof(deviceBuff[devNo].motorData[0]))
                deviceBuff[devNo].motorData[scheduleRequest.module].scheduleTime = scheduleRequest.deadline;
			Link_RecordPacket(devNo, 0);
			deviceBuff[devNo].lastCommTick = xTaskGetTickCount();
			deviceBuff[devNo].lastCommTime = systemInfo.currentTime;
			deviceBuff[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
		}
	}
}

void handleSyncTime(LoraPackage_t fwPackage) {
	if ((fwPackage.netId != systemInfo.netId) ||
			(fwPackage.nodeId == LORA_NODE_ID_DEFAULT || fwPackage.nodeId == LORA_NODE_ID_BROADCAST) ||
			(fwPackage.code != CMD_SYNC_TIME)) {
		return;
	}
	PayloadCommonResponse_t payloadResponse;
	if (fwPackage.type != LORA_PACKAGE_TYPE_RESPONSE || fwPackage.byteCount !=
		LORA_PACKAGE_HEADER_SIZE + sizeof(payloadResponse) + LORA_PACKAGE_CRC_SIZE) return;
	memcpy(&payloadResponse, &fwPackage.rawData[LORA_PACKAGE_HEADER_SIZE], sizeof(payloadResponse));
	
	for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo ++) {
		if (deviceBuff[devNo].isDelete == 0 && deviceBuff[devNo].nodeId == fwPackage.nodeId) {
			Link_RecordPacket(devNo, 0);
			deviceBuff[devNo].lastCommTick = xTaskGetTickCount();
			deviceBuff[devNo].lastCommTime =systemInfo.currentTime;
			deviceBuff[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);
		}
	}
}
void handleGetFactoryInfo(LoraPackage_t fwPackage){
	if ((fwPackage.netId != systemInfo.netId) ||
			(fwPackage.nodeId != LORA_NODE_ID_DEFAULT && fwPackage.nodeId != LORA_NODE_ID_BROADCAST) ||
			(fwPackage.code != CMD_GET_FACTORY_INFO)) {
		return;
	}
	PayloadResponseGetFactoryInfo_t payloadResponse;
	if (Comm_ParsePayloadResGetFactoryInfo(fwPackage, &payloadResponse) == FALSE)
		return;
	
	if (systemInfo.findDeviceStep == STEP_PROCESSING) {
		systemInfo.findDeviceStep = STEP_DONE;
		
		if (payloadResponse.result)  {
			systemInfo.findDeviceResult = PROCESS_PASSED;
			systemInfo.findDevicefactoryInfo.serial = payloadResponse.serial;
			systemInfo.findDevicefactoryInfo.deviceType = payloadResponse.deviceType;
			systemInfo.findDevicefactoryInfo.numberOfModule = payloadResponse.numberOfModule;
		}
		else {
			systemInfo.findDeviceResult = PROCESS_FAILED;
		}
	}
}
void handleConfigConnection(LoraPackage_t fwPackage) {
	if ((fwPackage.netId != systemInfo.netId) ||
			(fwPackage.nodeId != LORA_NODE_ID_DEFAULT && fwPackage.nodeId != LORA_NODE_ID_BROADCAST) ||
			(fwPackage.code != CMD_CONFIG_CONNECTION)) {
		return;
	}
	PayloadResponseConfigConnection_t payloadResponse;
	if (Comm_ParsePayloadResConfigConnection(fwPackage, &payloadResponse) == FALSE)
		return;
	
	if (systemInfo.configDeviceStep == STEP_PROCESSING) {
		systemInfo.configDeviceStep = STEP_DONE;
		if (payloadResponse.serial == systemInfo.findDevicefactoryInfo.serial) {
			if (payloadResponse.result == TRUE)  {
				systemInfo.configDeviceResult = PROCESS_PASSED;
			}
			else {
				systemInfo.configDeviceResult = PROCESS_FAILED;
			}
		}
		else {
			//
		}
	}
}
void handleAlarmMessage(LoraPackage_t fwPackage)
{
	if ((fwPackage.netId != systemInfo.netId) ||
			(fwPackage.nodeId == LORA_NODE_ID_DEFAULT || fwPackage.nodeId == LORA_NODE_ID_BROADCAST) ||
			(fwPackage.code != CMD_ALARM_MESSAGE)) {
		return;
	}
	PayloadRequestAlarm_t payloadRequest;
	if (Comm_ParsePayloadReqAlarm(fwPackage, &payloadRequest) == FALSE)
		return;
	if (fwPackage.type != LORA_PACKAGE_TYPE_REQUEST || payloadRequest.moduleIndex != 0) return;
	Link_RecordNodePacket(fwPackage.nodeId, 0);
	
	for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo ++) 
	{
		if (deviceBuff[devNo].isDelete == 0 && deviceBuff[devNo].nodeId == fwPackage.nodeId) {
			deviceBuff[devNo].motorData[0].isAlarmOccurred = TRUE;
			Power_WakeForError();
			deviceBuff[devNo].motorData[0].alarm = (AlarmName_t)payloadRequest.alarmCode;
			deviceBuff[devNo].motorData[0].alarmTime = payloadRequest.alarmTime;
			deviceBuff[devNo].lastCommTick = xTaskGetTickCount();
			deviceBuff[devNo].lastCommTime =systemInfo.currentTime;
			deviceBuff[devNo].rssi = LoRa_getRSSI(&sLoRaCtrl);

		}
	}
}
