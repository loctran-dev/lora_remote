#include "utility.h"

uint8_t Util_CalculateCRC(uint8_t *packet, uint32_t size)
{
    uint32_t i;
    uint8_t ck;

    for (ck = 0, i = 0; i < size; i++)
    {
        ck += packet[i];
    }
    return (~ck + 1);
}

uint32_t Util_GetUnixTimestamp(RTC_TimeTypeDef *sTime, RTC_DateTypeDef *sDate)
{
    static const uint16_t beforeMonth[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    uint32_t year = sDate->Year;
    uint32_t days = year * 365 + (year + 3) / 4 + beforeMonth[sDate->Month - 1] + sDate->Date - 1;
    if (!(year & 3) && sDate->Month > 2)
    {
        days++;
    }
    return 946684800u + days * 86400u + sTime->Hours * 3600u + sTime->Minutes * 60u + sTime->Seconds;
}

void Util_ConvertTimestampToRTC(uint32_t timestamp, RTC_TimeTypeDef *sTime, RTC_DateTypeDef *sDate)
{
    static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint32_t days, rem, year = 0, month = 0;
    /* RTC calendar supports years 2000..2099. */
    if (timestamp < 946684800u)
    {
        timestamp = 946684800u;
    }
    if (timestamp > 4102444799u)
    {
        timestamp = 4102444799u;
    }
    days = (timestamp - 946684800u) / 86400;
    rem = timestamp % 86400;

    sTime->Hours = rem / 3600;
    rem %= 3600;
    sTime->Minutes = rem / 60;
    sTime->Seconds = rem % 60;

    sDate->WeekDay = (days + 5) % 7 + 1;
    while (days >= 365u + !(year & 3))
    {
        days -= 365u + !(year & 3);
        year++;
    }
    while (month < 11)
    {
        uint32_t length = monthDays[month] + (month == 1 && !(year & 3));
        if (days < length)
        {
            break;
        }
        days -= length;
        month++;
    }
    sDate->Year = year;
    sDate->Month = month + 1;
    sDate->Date = days + 1;
}
