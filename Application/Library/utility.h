/**
 * @file    utility.h
 * @brief   Frame checksum, CRC-32 and Unix time conversion for the RTC calendar.
 *
 * @defgroup util Utilities
 * @{
 */
#ifndef UTILITY_H
#define UTILITY_H
#include "stdint.h"
#include "stm32f1xx_hal.h"

#define TRUE  1 ///< Boolean true used across the project.
#define FALSE 0 ///< Boolean false used across the project.

uint8_t Util_CalculateCRC(uint8_t *packet, uint32_t size);
uint32_t Util_GetUnixTimestamp(RTC_TimeTypeDef *sTime, RTC_DateTypeDef *sDate);
void Util_ConvertTimestampToRTC(uint32_t timestamp, RTC_TimeTypeDef *sTime, RTC_DateTypeDef *sDate);
uint32_t Util_Crc32(const void *data, uint32_t size);

#endif
/** @} */
