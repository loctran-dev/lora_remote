#ifndef UTILITY_H
#define UTILITY_H
#include "stdint.h"
#include "stm32f1xx_hal.h"
#define TRUE 1
#define FALSE 0
uint8_t calculateCRC(uint8_t *packet, uint32_t size);
uint32_t getUnixTimestamp(RTC_TimeTypeDef *sTime, RTC_DateTypeDef *sDate);
void convertTimestampToRTC(uint32_t timestamp, RTC_TimeTypeDef *sTime, RTC_DateTypeDef *sDate);
#endif
