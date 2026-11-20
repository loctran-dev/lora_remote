#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H
#include "stdint.h"
#include "communicationLora.h"

typedef void (*CmdHandler_t)(LoraPackage_t fwPackage);
void handleControlPower(LoraPackage_t fwPackage);
void handleGetStatus(LoraPackage_t fwPackage) ;
void handleSetSchedule(LoraPackage_t fwPackage);
void handleSyncTime(LoraPackage_t fwPackage);
void handleGetFactoryInfo(LoraPackage_t fwPackage);
void handleConfigConnection(LoraPackage_t fwPackage);
void handleAlarmMessage(LoraPackage_t fwPackage);
void handleSetConfig(LoraPackage_t fwPackage);
uint8_t sendCurrentModuleConfig(void);

enum { TIMER_IDLE, TIMER_WAITING, TIMER_CONFIRMED, TIMER_REJECTED, TIMER_TIMEOUT, TIMER_QUEUE_ERROR };
typedef struct {
    uint32_t syncTime, deadline, retryAt;
    uint16_t transaction;
    uint8_t net, node, module, state, attempts, txOk;
} ScheduleRequestState_t;
extern volatile ScheduleRequestState_t scheduleRequest;
void Schedule_Track(const LoraPackage_t *packet, const PayloadRequestSetSchedule_t *payload);
void Schedule_QueueFailed(void);
void Schedule_Tick(void);
void Schedule_CancelForNode(uint8_t node);
uint8_t Schedule_ShouldTransmit(const LoraPackage_t *packet);
void Schedule_TxFinished(const LoraPackage_t *packet, uint8_t success);
uint8_t Schedule_StateForNode(uint8_t node);
typedef struct {
    uint8_t cmdCode;
    CmdHandler_t handler;
} CmdMapping_t;

extern const CmdMapping_t cmdTable[10];

#endif
