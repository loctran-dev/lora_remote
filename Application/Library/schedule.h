#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "communication_lora.h"
#include "stdint.h"

typedef enum Schedule_State_e
{
    eSchedule_State_Idle,
    eSchedule_State_Waiting,
    eSchedule_State_Confirmed,
    eSchedule_State_Rejected,
    eSchedule_State_Timeout,
    eSchedule_State_QueueError
} Schedule_State_e;

typedef struct Schedule_RequestState_t
{
    uint32_t syncTime;
    uint32_t deadline;
    uint32_t retryAt;
    uint16_t transaction;
    uint8_t net;
    uint8_t node;
    uint8_t module;
    uint8_t attempts;
    uint8_t txOk;
    Schedule_State_e state;
} Schedule_RequestState_t;

void Schedule_Track(const LoraPackage_t *packet, const PayloadRequestSetSchedule_t *payload);
void Schedule_QueueFailed(void);
void Schedule_Tick(void);
void Schedule_CancelForNode(uint8_t node);
uint8_t Schedule_ShouldTransmit(const LoraPackage_t *packet);
void Schedule_TxFinished(const LoraPackage_t *packet, uint8_t success);
Schedule_State_e Schedule_GetStateForNode(uint8_t node);
void Schedule_HandleStatus(const LoraPackage_t *packet, const PayloadResponseGetStatus_t *payload);
uint8_t Schedule_HandleResponse(const LoraPackage_t *packet, const PayloadCommonResponse_t *payload,
    uint32_t *deadline);
uint8_t Schedule_IsWaiting(void);
void Schedule_GetRequestState(Schedule_RequestState_t *state);

#endif
