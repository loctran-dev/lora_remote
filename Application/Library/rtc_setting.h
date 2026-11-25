/**
 * @file    rtc_setting.h
 * @brief   Reading and setting the RTC, and keeping the calendar date across resets.
 *
 * The STM32F1 RTC only counts seconds; the HAL keeps the date in RAM. The date
 * is therefore saved in backup registers DR2..DR4 and restored at start-up,
 * adding the whole days the counter ran while the remote was off or stopped.
 *
 * @defgroup rtc RTC
 * @{
 */
#ifndef RTC_SETTING_H
#define RTC_SETTING_H
#include <stdint.h>

/** @brief Result of Rtc_Set(). */
typedef enum Rtc_SetResult_e
{
    eRtc_SetResult_Ok,    ///< Time set and read back.
    eRtc_SetResult_Range, ///< Outside 2000-01-01 .. 2099-12-31, or not a valid command.
    eRtc_SetResult_Busy,  ///< A schedule is pending or running on a device.
    eRtc_SetResult_Io     ///< HAL error or the read-back time does not match.
} Rtc_SetResult_e;

/** @name Backup of the calendar date */
/** @{ */
void Rtc_SaveDate(void);
void Rtc_RestoreDate(void);
/** @} */

/** @name Read and set the RTC */
/** @{ */
uint32_t Rtc_Read(void);
uint8_t Rtc_Set(uint32_t timestamp);
/** @} */

/** @name USB "TIME <unix>\n" command */
/** @{ */
uint8_t Rtc_Parse(const uint8_t *command, uint32_t length, uint32_t *timestamp);
/** @} */

#endif
/** @} */
