#include "schedule.h"
#include "cmsis_os.h"
#include "define.h"
#include "main.h"
#include "string.h"
#include "utility.h"

extern osMessageQueueId_t txMsgQueueHandle;
extern SystemInfo_t systemInfo;

static volatile Schedule_RequestState_t scheduleRequest;

/**
 * @brief  Start tracking a schedule request that is about to be queued.
 *
 * Replaces any request tracked before. The first resend is due 3 s later.
 *
 * @param  packet   The CMD_SET_SCHEDULE frame; its transaction id is reused.
 * @param  payload  Its payload.
 */
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

/** @brief Mark the tracked request as not queued (eSchedule_State_QueueError). */
void Schedule_QueueFailed(void)
{
    if (scheduleRequest.state == eSchedule_State_Waiting)
    {
        scheduleRequest.state = eSchedule_State_QueueError;
    }
}

/**
 * @brief  Stop tracking the request for one motor.
 * @param  node    Node id of its device.
 * @param  module  Module index; another motor's request on the same node is kept.
 * @note   Called before a power command, which replaces that motor's schedule.
 */
void Schedule_CancelForMotor(uint8_t node, uint8_t module)
{
    if (scheduleRequest.node == node && scheduleRequest.module == module)
    {
        scheduleRequest.state = eSchedule_State_Idle;
    }
}

/**
 * @brief  State of the request for one motor.
 * @param  node    Node id of its device.
 * @param  module  Module index.
 * @return The tracked state, or eSchedule_State_Idle for another motor.
 */
Schedule_State_e Schedule_GetStateForMotor(uint8_t node, uint8_t module)
{
    if (scheduleRequest.node != node || scheduleRequest.module != module)
    {
        return eSchedule_State_Idle;
    }

    return scheduleRequest.state;
}

/**
 * @brief  Filter stale CMD_SET_SCHEDULE requests before they are sent.
 * @param  packet  Packet taken from txMsgQueue.
 * @return TRUE to send it: every packet except a schedule request that is
 *         not the one being tracked.
 * @note   Called by loraSendTask.
 */
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

/**
 * @brief  Count a sent schedule request and restart the 3 s resend timer.
 * @param  packet   Packet just transmitted.
 * @param  success  Radio result.
 * @note   Called by loraSendTask.
 */
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

/**
 * @brief  Resend the tracked request when due, or time it out.
 * @note   Call from systemTask. A timeout gives a long beep.
 */
void Schedule_Tick(void)
{
    PayloadRequestSetSchedule_t payload;

    if (scheduleRequest.state != eSchedule_State_Waiting ||
        (int32_t)(HAL_GetTick() - scheduleRequest.retryAt) < 0)
    {
        return;
    }

    if (scheduleRequest.attempts >= 3 ||
        (scheduleRequest.deadline && systemInfo.currentTime >= scheduleRequest.deadline))
    {
        scheduleRequest.state = eSchedule_State_Timeout;
        Main_SetBuzzerMode(eBuzzer_Mode_LongBeep);
        return;
    }

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

/**
 * @brief  Confirm the request when a status report of the same motor shows the same schedule.
 * @param  packet   Received CMD_GET_STATUS response.
 * @param  payload  Its payload.
 */
void Schedule_HandleStatus(const LoraPackage_t *packet, const PayloadResponseGetStatus_t *payload)
{
    if ((scheduleRequest.state != eSchedule_State_Waiting &&
         scheduleRequest.state != eSchedule_State_Timeout) ||
        packet->nodeId != scheduleRequest.node || payload->moduleIndex != scheduleRequest.module ||
        payload->scheduleTime != scheduleRequest.deadline ||
        (scheduleRequest.deadline && payload->powerState != eMotor_PowerState_On))
    {
        return;
    }

    scheduleRequest.state = eSchedule_State_Confirmed;
}

/**
 * @brief  Apply the device's answer to the tracked request.
 * @param  packet    Received CMD_SET_SCHEDULE response.
 * @param  payload   Its payload.
 * @param  deadline  Set to the requested switch-off time when the answer matches; may be NULL.
 * @return TRUE if the answer belongs to the tracked request.
 */
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

/**
 * @brief  Whether a request is still waiting for the device.
 * @return TRUE while waiting; the remote does not sleep then.
 */
uint8_t Schedule_IsWaiting(void)
{
    return scheduleRequest.state == eSchedule_State_Waiting;
}

/**
 * @brief  Copy the tracked request, for diagnostics.
 * @param  state  Destination; ignored if NULL.
 */
void Schedule_GetRequestState(Schedule_RequestState_t *state)
{
    if (state == NULL)
    {
        return;
    }

    *state = scheduleRequest;
}
