#ifndef RTC_SETTING_H
#define RTC_SETTING_H
#include <stdint.h>
enum { RTC_SET_OK, RTC_SET_RANGE, RTC_SET_BUSY, RTC_SET_IO };
uint8_t Rtc_Set(uint32_t timestamp);
uint32_t Rtc_Read(void);
void Rtc_SaveDate(void);
void Rtc_RestoreDate(void);
uint8_t Rtc_Parse(const uint8_t *command,uint32_t length,uint32_t *timestamp);
#endif
