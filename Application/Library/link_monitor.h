/**
 * @file    link_monitor.h
 * @brief   Link supervision of the paired devices and the link settings.
 *
 * Keeps an uptime clock in seconds from the RTC (it keeps counting in STOP)
 * and probes, one device at a time, every device that has been silent for
 * longer than the connection timeout: up to three GET_STATUS requests,
 * 10 s apart, before the device is marked as a connection error. After an
 * RTC change a status sync probes every paired device once. Link_Tick() is
 * the only source of automatic GET_STATUS requests.
 *
 * The idle, connection-timeout and auto-status settings live in the
 * checksummed extension of SystemStorageData_t.
 *
 * @defgroup link Link monitor
 * @{
 */
#ifndef LINK_MONITOR_H
#define LINK_MONITOR_H
#include <stdint.h>
#include "communication_lora.h"

/** @brief Link state of one paired device (index as in devicesInfo). */
typedef struct
{
    uint32_t lastRx;           ///< Link_Now() of the last packet from the device [s].
    uint32_t lastStatus;       ///< Link_Now() of the last status packet [s].
    uint32_t retryAt;          ///< Link_Now() before which a failed device is not probed again [s]; 0 = no back-off.
    uint32_t statusGeneration; ///< Incremented on every status packet; ends a probe when it changes.
    uint8_t node;              ///< Node id this state belongs to; a change resets the state.
} Link_DeviceState_t;

/** @brief The probe in progress and the probe statistics. */
typedef struct
{
    uint32_t requests;         ///< GET_STATUS requests queued.
    uint32_t confirmations;    ///< Probes answered by the device.
    uint32_t failures;         ///< Probes that ran out of attempts.
    uint32_t receivedStatus;   ///< Status packets received from any device.
    uint32_t deadline;         ///< HAL_GetTick() at which the current attempt times out [ms].
    uint16_t transaction;      ///< Transaction id of the current GET_STATUS request.
    int8_t device;             ///< Device being probed, or -1 when idle.
    uint8_t attempts;          ///< Attempts used by the current probe (at most 3).
    uint8_t state;             ///< 0 = send next attempt, 1 = queued, 2 = sent, waiting for the reply.
} Link_ProbeState_t;

/** @brief Per-device link state, indexed like devicesInfo; shown on the device info screen. */
extern Link_DeviceState_t linkDevices[];
/** @brief Probe state; also read by the USB console. */
extern Link_ProbeState_t linkProbe;

/* Settings */

uint16_t Link_GetIdleSeconds(void);
uint16_t Link_GetTimeoutSeconds(void);
uint16_t Link_GetAutoStatus(uint8_t id);
void Link_SetAutoStatus(uint8_t id, uint16_t seconds);
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

void Link_RecordStatusReply(uint8_t id, uint16_t transaction);
void Link_RecordPacket(uint8_t id, uint8_t status);
void Link_RecordNodePacket(uint8_t node, uint8_t status);

/* Transmit path */

uint8_t Link_ShouldTransmit(const LoraPackage_t *packet);
void Link_TxFinished(const LoraPackage_t *packet, uint8_t success);

/* Periodic processing */

void Link_Tick(void);

#endif
/** @} */
