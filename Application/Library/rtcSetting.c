#include "rtcSetting.h"
#include "main.h"
#include "define.h"
#include "utility.h"
#include "commandHandlers.h"
#include "linkMonitor.h"
#include "powerSleep.h"
extern RTC_HandleTypeDef hrtc;
extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];
#define DATE_MAGIC 0xD471u
/* F1 HAL keeps its calendar in RAM. Preserve it in free BKP DR2..DR4;
   DR1 is the existing RTC-init marker, DR10 belongs to the bootloader. */
void Rtc_SaveDate(void)
{
    RTC_DateTypeDef *d=&systemInfo.date;
    if (d->Year>99 || !d->Month || d->Month>12 || !d->Date || d->Date>31) return;
    uint16_t packed=((uint16_t)d->Year<<9)|((uint16_t)d->Month<<5)|d->Date;
    if (HAL_RTCEx_BKUPRead(&hrtc,RTC_BKP_DR4)==DATE_MAGIC &&
        HAL_RTCEx_BKUPRead(&hrtc,RTC_BKP_DR2)==packed &&
        HAL_RTCEx_BKUPRead(&hrtc,RTC_BKP_DR3)==(uint16_t)~packed) return;
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    HAL_RTCEx_BKUPWrite(&hrtc,RTC_BKP_DR4,0);
    HAL_RTCEx_BKUPWrite(&hrtc,RTC_BKP_DR2,packed);
    HAL_RTCEx_BKUPWrite(&hrtc,RTC_BKP_DR3,(uint16_t)~packed);
    HAL_RTCEx_BKUPWrite(&hrtc,RTC_BKP_DR4,DATE_MAGIC);
}
void Rtc_RestoreDate(void)
{
    uint16_t packed=HAL_RTCEx_BKUPRead(&hrtc,RTC_BKP_DR2);
    if (HAL_RTCEx_BKUPRead(&hrtc,RTC_BKP_DR4)!=DATE_MAGIC ||
        HAL_RTCEx_BKUPRead(&hrtc,RTC_BKP_DR3)!=(uint16_t)~packed) return;
    RTC_DateTypeDef date={0};
    RTC_TimeTypeDef time={0};
    date.Year=packed>>9; date.Month=(packed>>5)&15; date.Date=packed&31;
    if (date.Year>99 || !date.Month || date.Month>12 || !date.Date || date.Date>31) return;
    /* HAL_SetDate normalizes a counter over 24 h. Account for days spent
       stopped/off with the backup domain still powered BEFORE that happens. */
    uint32_t first,second,low;
    do { first=RTC->CNTH; low=RTC->CNTL; second=RTC->CNTH; } while (first!=second);
    uint32_t days=((second<<16)|low)/86400u;
    uint64_t restored=(uint64_t)getUnixTimestamp(&time,&date)+(uint64_t)days*86400u;
    if (restored>4102444799u) return;
    convertTimestampToRTC((uint32_t)restored,&time,&date);
    HAL_RTC_SetDate(&hrtc,&date,RTC_FORMAT_BIN);
}
uint32_t Rtc_Read(void)
{
    HAL_RTC_GetTime(&hrtc,&systemInfo.time,RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc,&systemInfo.date,RTC_FORMAT_BIN);
    systemInfo.currentTime=getUnixTimestamp(&systemInfo.time,&systemInfo.date);
    Rtc_SaveDate();
    return systemInfo.currentTime;
}
uint8_t Rtc_Set(uint32_t timestamp)
{
    if (timestamp<946684800u || timestamp>4102444799u) return RTC_SET_RANGE;
    if (scheduleRequest.state==TIMER_WAITING) return RTC_SET_BUSY;
    for (uint8_t i=0;i<MAXIMUM_DEVICE;i++)
        for (uint8_t module=0;module<sizeof(deviceBuff[i].motorData)/sizeof(deviceBuff[i].motorData[0]);module++)
            if (!deviceBuff[i].isDelete && deviceBuff[i].motorData[module].pwrState==MOTOR_POWER_ON &&
                deviceBuff[i].motorData[module].scheduleTime) return RTC_SET_BUSY;
    RTC_TimeTypeDef time={0};
    RTC_DateTypeDef date={0};
    convertTimestampToRTC(timestamp,&time,&date);
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    if (HAL_RTC_SetTime(&hrtc,&time,RTC_FORMAT_BIN)!=HAL_OK ||
        HAL_RTC_SetDate(&hrtc,&date,RTC_FORMAT_BIN)!=HAL_OK) return RTC_SET_IO;
    HAL_RTCEx_BKUPWrite(&hrtc,RTC_BKP_DR1,0x3210);
    uint32_t actual=Rtc_Read();
    Link_ClockRebase(Power_ReadRtcSecond());
    Power_UserActivity();
    if (actual<timestamp || actual-timestamp>1u) return RTC_SET_IO;
    Link_RequestStatusSync();
    return RTC_SET_OK;
}
uint8_t Rtc_Parse(const uint8_t *command,uint32_t length,uint32_t *timestamp)
{
    if (length<7 || length>16 || command[length-1]!='\n') return 0;
    if (command[0]!='T' || command[1]!='I' || command[2]!='M' || command[3]!='E' || command[4]!=' ') return 0;
    uint32_t value=0;
    for (uint32_t i=5;i<length-1;i++) {
        uint8_t digit=command[i]-'0';
        if (digit>9 || value>429496729u || (value==429496729u && digit>5)) return 0;
        value=value*10+digit;
    }
    *timestamp=value;
    return 1;
}
