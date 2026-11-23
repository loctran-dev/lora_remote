#include "schedule.h"
#include "cmsis_os.h"
#include "define.h"
#include "main.h"
#include "string.h"
#include "utility.h"

extern osMessageQueueId_t txMsgQueueHandle;
extern SystemInfo_t systemInfo;

static volatile Schedule_RequestState_t scheduleRequest;

void Schedule_Track(const LoraPackage_t *packet, const PayloadRequestSetSchedule_t *payload)
{
    scheduleRequest.state = eSchedule_State_Idle;
    scheduleRequest.syncTime = payload->syncTime;
    scheduleRequest.deadline = payload->scheduleTime;
    scheduleRequest.transaction = packet->transactionID;
    scheduleRequest.net = packet->netId;
    scheduleRequest.node = packet->nodeId;
    scheduleRequest.module = payload->moduleIndex;
    scheduleRequest.attempts = 1;
    scheduleRequest.txOk = 0;
    scheduleRequest.retryAt = HAL_GetTick() + 3000u;
    scheduleRequest.state = eSchedule_State_Waiting;
}

void Schedule_QueueFailed(void)
{
    if (scheduleRequest.state == eSchedule_State_Waiting)
    {
        scheduleRequest.state = eSchedule_State_QueueError;
    }
}

void Schedule_CancelForNode(uint8_t node)
{
    if (scheduleRequest.node == node)
    {
        scheduleRequest.state = eSchedule_State_Idle;
    }
}

Schedule_State_e Schedule_GetStateForNode(uint8_t node)
{
    if (scheduleRequest.node != node)
    {
        return eSchedule_State_Idle;
    }

    return scheduleRequest.state;
}

uint8_t Schedule_ShouldTransmit(const LoraPackage_t *packet)
{
    if (packet->code != CMD_SET_SCHEDULE || packet->type != LORA_PACKAGE_TYPE_REQUEST)
    {
        return TRUE;
    }

    return scheduleRequest.state == eSchedule_State_Waiting &&
        packet->netId == scheduleRequest.net && packet->nodeId == scheduleRequest.node &&
        packet->transactionID == scheduleRequest.transaction;
}

void Schedule_TxFinished(const LoraPackage_t *packet, uint8_t success)
{
    if (packet->code != CMD_SET_SCHEDULE || !Schedule_ShouldTransmit(packet))
    {
        return;
    }

    if (success)
    {
        scheduleRequest.txOk++;
    }

    scheduleRequest.retryAt = HAL_GetTick() + 3000u;
}

void Schedule_Tick(void)
{
    if (scheduleRequest.state != eSchedule_State_Waiting ||
        (int32_t)(HAL_GetTick() - scheduleRequest.retryAt) < 0)
    {
        return;
    }

    if (scheduleRequest.attempts >= 3 ||
        (scheduleRequest.deadline && systemInfo.currentTime >= scheduleRequest.deadline))
    {
        scheduleRequest.state = eSchedule_State_Timeout;
        Main_SetBuzzerMode(BUZZER_LONG_BEEP);
        return;
    }

    PayloadRequestSetSchedule_t payload;
    payload.moduleIndex = scheduleRequest.module;
    payload.syncTime = scheduleRequest.syncTime;
    payload.scheduleTime = scheduleRequest.deadline;

    LoraPackage_t packet = Comm_CreateReqSetSchedule(scheduleRequest.net, scheduleRequest.node,
        payload);
    packet.transactionID = scheduleRequest.transaction;
    memcpy(packet.rawData + 6, &packet.transactionID, sizeof(packet.transactionID));
    packet.checkSum = Util_CalculateCRC(packet.rawData, packet.length);
    packet.rawData[packet.byteCount - 1] = packet.checkSum;

    scheduleRequest.attempts++;
    scheduleRequest.retryAt = HAL_GetTick() + 3000u;

    if (osMessageQueuePut(txMsgQueueHandle, &packet, 0, 0) != osOK)
    {
        Schedule_QueueFailed();
    }
}

void Schedule_HandleStatus(const LoraPackage_t *packet, const PayloadResponseGetStatus_t *payload)
{
    if ((scheduleRequest.state != eSchedule_State_Waiting &&
         scheduleRequest.state != eSchedule_State_Timeout) ||
        packet->nodeId != scheduleRequest.node || payload->scheduleTime != scheduleRequest.deadline ||
        (scheduleRequest.deadline && payload->powerState != MOTOR_POWER_ON))
    {
        return;
    }

    scheduleRequest.state = eSchedule_State_Confirmed;
}

uint8_t Schedule_HandleResponse(const LoraPackage_t *packet, const PayloadCommonResponse_t *payload,
    uint32_t *deadline)
{
    if ((scheduleRequest.state != eSchedule_State_Waiting &&
         scheduleRequest.state != eSchedule_State_Timeout) ||
        packet->netId != scheduleRequest.net || packet->nodeId != scheduleRequest.node ||
        packet->transactionID != scheduleRequest.transaction ||
        payload->moduleIndex != scheduleRequest.module)
    {
        return FALSE;
    }

    scheduleRequest.state = payload->result == TRUE ? eSchedule_State_Confirmed :
        eSchedule_State_Rejected;

    if (deadline != NULL)
    {
        *deadline = scheduleRequest.deadline;
    }

    return TRUE;
}

uint8_t Schedule_IsWaiting(void)
{
    return scheduleRequest.state == eSchedule_State_Waiting;
}

void Schedule_GetRequestState(Schedule_RequestState_t *state)
{
    if (state == NULL)
    {
        return;
    }

    *state = scheduleRequest;
}
