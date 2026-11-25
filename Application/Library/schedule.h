/**
 * @file    schedule.h
 * @brief   Delivery of CMD_SET_SCHEDULE requests with retries and confirmation.
 *
 * Tracks one schedule request at a time. The request is resent with the same
 * transaction id every 3 s, at most 3 times, until the device accepts or
 * rejects it, or a status report shows the same switch-off time. It times out
 * (long beep) after the last attempt or once the switch-off time has passed.
 *
 * @defgroup schedule Schedule request
 * @{
 */
#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "communication_lora.h"
#include "stdint.h"

/** @brief State of the tracked schedule request. */
typedef enum Schedule_State_e
{
    eSchedule_State_Idle,      ///< No request, or cancelled by a new power command.
    eSchedule_State_Waiting,   ///< Sent or being resent; waiting for the device.
    eSchedule_State_Confirmed, ///< Accepted by the device or seen in its status.
    eSchedule_State_Rejected,  ///< The device answered with result 0.
    eSchedule_State_Timeout,   ///< No answer after the last attempt or past the deadline.
    eSchedule_State_QueueError ///< The request could not be queued for sending.
} Schedule_State_e;

/** @brief The tracked schedule request. */
typedef struct Schedule_RequestState_t
{
    uint32_t syncTime;      ///< Remote Unix time sent with the request.
    uint32_t deadline;      ///< Requested switch-off time (Unix); 0 = cancel.
    uint32_t retryAt;       ///< HAL_GetTick() of the next resend [ms].
    uint16_t transaction;   ///< Transaction id reused by every resend.
    uint8_t net;            ///< Destination network id.
    uint8_t node;           ///< Destination node id.
    uint8_t module;         ///< Motor module, 0.
    uint8_t attempts;       ///< Requests queued so far (at most 3).
    uint8_t txOk;           ///< Transmissions reported successful by the radio.
    Schedule_State_e state; ///< Current state.
} Schedule_RequestState_t;

/** @name Request lifecycle */
/** @{ */
void Schedule_Track(const LoraPackage_t *packet, const PayloadRequestSetSchedule_t *payload);
void Schedule_QueueFailed(void);
void Schedule_CancelForNode(uint8_t node);
Schedule_State_e Schedule_GetStateForNode(uint8_t node);
/** @} */

/** @name Transmit path */
/** @{ */
uint8_t Schedule_ShouldTransmit(const LoraPackage_t *packet);
void Schedule_TxFinished(const LoraPackage_t *packet, uint8_t success);
void Schedule_Tick(void);
/** @} */

/** @name Receive path */
/** @{ */
void Schedule_HandleStatus(const LoraPackage_t *packet, const PayloadResponseGetStatus_t *payload);
uint8_t Schedule_HandleResponse(const LoraPackage_t *packet, const PayloadCommonResponse_t *payload,
    uint32_t *deadline);
/** @} */

/** @name State queries */
/** @{ */
uint8_t Schedule_IsWaiting(void);
void Schedule_GetRequestState(Schedule_RequestState_t *state);
/** @} */

#endif
/** @} */
