#ifndef POWER_SLEEP_H
#define POWER_SLEEP_H
#include <stdint.h>

typedef struct
{
    uint32_t stops;
    uint32_t rtcWakes;
    uint32_t radioWakes;
    uint32_t buttonWakes;
    uint32_t lastUser;
    uint32_t nextRtc;
    uint32_t lastUsbSof;
    volatile uint8_t stopping;
    volatile uint8_t rxPending;
    volatile uint8_t txBusy;
    volatile uint8_t rtcPending;
    volatile uint8_t buttonPending;
} Power_SleepState_t;

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
