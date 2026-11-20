#ifndef POWER_SLEEP_H
#define POWER_SLEEP_H
#include <stdint.h>
typedef struct {
    uint32_t stops, rtcWakes, radioWakes, buttonWakes;
    uint32_t lastUser, nextRtc, lastUsbSof;
    volatile uint8_t stopping, rxPending, txBusy, rtcPending, buttonPending;
} PowerSleepState_t;
extern PowerSleepState_t powerSleep;
uint32_t Power_ReadRtcSecond(void);
void Power_Init(void);
void Power_UserActivity(void);
void Power_RtcWakeISR(void);
void Power_RadioWakeISR(void);
void Power_ButtonWakeISR(void);
void Power_UsbSofISR(void);
void Power_RxProcessed(void);
void Power_Tick(void);
void Power_TryStop(void);
void Power_WakeForError(void);
#endif
