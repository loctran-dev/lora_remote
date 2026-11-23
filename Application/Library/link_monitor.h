#ifndef LINK_MONITOR_H
#define LINK_MONITOR_H
#include <stdint.h>
#include "communication_lora.h"

#define LINK_CONFIRM_ATTEMPTS      3
#define LINK_REPLY_TIMEOUT_SECONDS 10

typedef struct
{
    uint32_t lastRx, lastStatus, retryAt, statusGeneration;
    uint8_t node;
} LinkDeviceState_t;

typedef struct
{
    uint32_t requests, confirmations, failures, receivedStatus;
    uint32_t deadline;
    uint16_t transaction;
    int8_t device;
    uint8_t attempts, state;
} LinkProbeState_t;

extern LinkDeviceState_t linkDevices[];
extern LinkProbeState_t linkProbe;

/* Settings */
uint16_t Link_GetIdleSeconds(void);
uint16_t Link_GetTimeoutSeconds(void);
uint16_t Link_GetAutoStatus(uint8_t device);
void Link_SetAutoStatus(uint8_t device, uint16_t seconds);
void Link_SetIdleSeconds(uint16_t seconds);
void Link_SetTimeoutSeconds(uint16_t seconds);

/* Initialisation and clock */
void Link_Init(uint32_t rtcSecond);
void Link_ClockStep(uint32_t rtcSecond);
uint32_t Link_Now(void);
void Link_ClockRebase(uint32_t rtcSecond);

/* Requests */
void Link_RequestScan(void);
uint8_t Link_IsBusy(void);
void Link_RequestStatusSync(void);

/* Received packets */
void Link_RecordStatusReply(uint8_t device, uint16_t transaction);
void Link_RecordPacket(uint8_t device, uint8_t status);
void Link_RecordNodePacket(uint8_t node, uint8_t status);

/* Transmit path */
uint8_t Link_ShouldTransmit(const LoraPackage_t *packet);
void Link_TxFinished(const LoraPackage_t *packet, uint8_t success);

/* Periodic processing */
void Link_Tick(void);

#endif
