/**
 * @file    power_sleep.h
 * @brief   Sleep policy and STOP-mode entry/exit for the remote.
 *
 * Power_Tick() moves the remote from NORMAL to SLEEP (backlight off) after
 * the idle timeout. Power_TryStop() then puts the MCU into STOP whenever
 * every peripheral is idle, and restores the clocks after a wake-up from
 * the RTC alarm, LoRa DIO0 or the LEFT/RIGHT/OFF buttons. Both run in
 * systemTask; the *ISR() functions only set flags for them.
 *
 * @defgroup power Power / sleep
 * @{
 */
#ifndef POWER_SLEEP_H
#define POWER_SLEEP_H
#include <stdint.h>

/** @brief Wake-up statistics and pending flags shared with the ISRs. */
typedef struct
{
    uint32_t stops;                 ///< Number of STOP entries.
    uint32_t rtcWakes;              ///< Wake-ups caused by RTC Alarm A.
    uint32_t radioWakes;            ///< Wake-ups caused by LoRa DIO0.
    uint32_t buttonWakes;           ///< Wake-ups caused by LEFT, RIGHT or OFF.
    uint32_t lastUser;              ///< Link_Now() of the last user action [s].
    uint32_t nextRtc;               ///< Link_Now() of the next sleep scan [s].
    uint32_t lastUsbSof;            ///< HAL_GetTick() of the last USB SOF [ms].
    volatile uint8_t stopping;      ///< 1 while STOP is being entered or left.
    volatile uint8_t rxPending;     ///< LoRa packet waiting; cleared by Power_RxProcessed().
    volatile uint8_t txBusy;        ///< 1 while loraSendTask is transmitting.
    volatile uint8_t rtcPending;    ///< RTC alarm fired; handled by Power_Tick().
    volatile uint8_t buttonPending; ///< Wake button pressed; handled by Power_Tick().
} Power_SleepState_t;

/** @brief Sleep state; also used by the tasks in main.c and the USB console. */
extern Power_SleepState_t powerSleep;

/* RTC clock */

uint32_t Power_ReadRtcSecond(void);

/* Initialisation and activity */

void Power_Init(void);
void Power_UserActivity(void);
void Power_WakeForError(void);

/* Wake sources (ISR context) */

void Power_RtcWakeISR(void);
void Power_RadioWakeISR(void);
void Power_ButtonWakeISR(void);
void Power_UsbSofISR(void);
void Power_RxProcessed(void);

/* Sleep policy */

void Power_Tick(void);
void Power_TryStop(void);

#endif
/** @} */
