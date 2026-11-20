#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H
#include "stdint.h"
#include "communicationLora.h"

typedef void (*CmdHandler_t)(LoraPackage_t fwPackage);
uint8_t CmdHandler_SendCurrentModuleConfig(void);

typedef enum CmdHandler_TimerState_e
{
    eCmdHandler_TimerState_Idle,
    eCmdHandler_TimerState_Waiting,
    eCmdHandler_TimerState_Confirmed,
    eCmdHandler_TimerState_Rejected,
    eCmdHandler_TimerState_Timeout,
    eCmdHandler_TimerState_QueueError
} CmdHandler_TimerState_e;

typedef struct {
    uint32_t syncTime, deadline, retryAt;
    uint16_t transaction;
    uint8_t net, node, module, attempts, txOk;
    CmdHandler_TimerState_e state;
} CmdHandler_ScheduleRequestState_t;

extern volatile CmdHandler_ScheduleRequestState_t scheduleRequest;
void CmdHandler_Schedule_Track(const LoraPackage_t *packet, const PayloadRequestSetSchedule_t *payload);
void CmdHandler_Schedule_QueueFailed(void);
void CmdHandler_Schedule_Tick(void);
void CmdHandler_Schedule_CancelForNode(uint8_t node);
uint8_t CmdHandler_Schedule_ShouldTransmit(const LoraPackage_t *packet);
void CmdHandler_Schedule_TxFinished(const LoraPackage_t *packet, uint8_t success);
CmdHandler_TimerState_e CmdHandler_Schedule_StateForNode(uint8_t node);
typedef struct {
    uint8_t cmdCode;
    CmdHandler_t handler;
} CmdMapping_t;

extern const CmdMapping_t cmdTable[10];

#endif
