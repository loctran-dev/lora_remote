#ifndef RTC_SETTING_H
#define RTC_SETTING_H
#include <stdint.h>

typedef enum Rtc_SetResult_e
{
    eRtc_SetResult_Ok,
    eRtc_SetResult_Range,
    eRtc_SetResult_Busy,
    eRtc_SetResult_Io
} Rtc_SetResult_e;

/* Backup of the calendar date */
void Rtc_SaveDate(void);
void Rtc_RestoreDate(void);

/* Read and set the RTC */
uint32_t Rtc_Read(void);
uint8_t Rtc_Set(uint32_t timestamp);

/* USB "TIME <unix>\n" command */
uint8_t Rtc_Parse(const uint8_t *command, uint32_t length, uint32_t *timestamp);

#endif
