#include "screens.h"
#include "main.h"
#include "rtc_setting.h"
#include "utility.h"
#include <stdio.h>

extern SystemInfo_t systemInfo;
/* Separate field textboxes: no cursor, no edits to the live RTC until Save. */
static RTC_TimeTypeDef editedTime;
static RTC_DateTypeDef editedDate;
static uint8_t selectedField, saveError;
static UG_TEXTBOX *boxes[]={&textbox_1,&textbox_2,&textbox_3,&textbox_4,&textbox_5,
    &textbox_6,&textbox_7,&textbox_8,&textbox_9,&textbox_10,&textbox_11,&textbox_12,
    &textbox_13,&textbox_14,&textbox_15,&textbox_16,&textbox_17,&textbox_18,&textbox_19,&textbox_20};
static void callback(UG_MESSAGE *message) { (void)message; }
static void nothing(void) {}
static uint8_t monthLength(uint8_t year, uint8_t month)
{
    static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month-1]+(month==2 && !(year&3));
}
static void text(uint8_t id,int x,int y,int xe,int ye,char *value,UG_COLOR color)
{
    UG_TextboxCreate(&window_1,boxes[id],id,x,y,xe,ye);
    UG_TextboxSetFont(&window_1,id,DEFAULT_FONT);
    UG_TextboxSetText(&window_1,id,value);
    UG_TextboxSetBackColor(&window_1,id,color);
    UG_TextboxSetForeColor(&window_1,id,C_WHITE);
    UG_TextboxSetAlignment(&window_1,id,ALIGN_CENTER);
    UG_TextboxSetScrollEnabled(&window_1,id,0);
}
static void back(void)
{
    UG_WindowHide(&window_1); UG_WindowDelete(&window_1);
    currentScreen=&generalSettingScreen;
    currentScreen->init(); currentScreen->render();
}
void DTS_Init(void)
{
    Rtc_Read();
    editedTime=systemInfo.time; editedDate=systemInfo.date;
    /* An unset clock starts at the first supported operational year. */
    if (editedDate.Year<24) editedDate.Year=24;
    selectedField=saveError=0;
    UG_WindowCreate(&window_1,obj_buff_wnd_1,MAX_OBJECTS,callback);
    UG_WindowSetStyle(&window_1,WND_STYLE_HIDE_TITLE);
    UG_WindowSetTitleHeight(&window_1,0); UG_WindowSetBackColor(&window_1,C_BLACK);
    UG_WindowSetXStart(&window_1,0); UG_WindowSetYStart(&window_1,0);
    UG_WindowSetXEnd(&window_1,127); UG_WindowSetYEnd(&window_1,159);
    Screen_StatusBarCreate();
    text(2,0,20,127,37,"Date & time",C_DARK_SLATE_BLUE);
    text(3,0,41,127,51,"DAY    MONTH   YEAR",C_BLACK);
    text(4,0,79,127,89,"HOUR    MIN     SEC",C_BLACK);
    for (uint8_t i=0;i<6;i++) {
        int x=4+(i%3)*42, y=i<3?55:93;
        text(5+i,x,y,x+31,y+20,"",C_BLACK);
    }
    text(11,37,55,45,75,"/",C_BLACK); text(12,79,55,87,75,"/",C_BLACK);
    text(13,37,93,45,113,":",C_BLACK); text(14,79,93,87,113,":",C_BLACK);
    text(15,0,117,127,127,"Hold CENTER: save",C_BLACK);
    text(16,0,130,127,140,"Hold RIGHT: cancel",C_BLACK);
    text(17,0,144,41,159,"+",C_RED);
    text(18,42,144,85,159,"Next",C_RED);
    text(19,86,144,127,159,"-",C_RED);
    UG_WindowShow(&window_1);
}
void DTS_Render(void)
{
    char value[24];
    uint16_t values[]={editedDate.Date,editedDate.Month,2000u+editedDate.Year,
        editedTime.Hours,editedTime.Minutes,editedTime.Seconds};
    for (uint8_t i=0;i<6;i++) {
        sprintf(value,i==2?"%04u":"%02u",values[i]);
        UG_TextboxSetText(&window_1,5+i,value);
        UG_TextboxSetBackColor(&window_1,5+i,i==selectedField?C_DARK_SLATE_BLUE:C_BLACK);
        UG_TextboxSetForeColor(&window_1,5+i,i==selectedField?C_WHITE:C_CYAN);
    }
    Screen_StatusBarUpdate();
    UG_TextboxSetText(&window_1,15,saveError==RTC_SET_BUSY?"Timer active: blocked":
        saveError?"RTC error; try again":"Hold CENTER: save");
    UG_TextboxSetForeColor(&window_1,15,saveError?C_RED:C_WHITE);
    UG_Update();
}
static void change(int8_t direction)
{
    uint8_t *field;
    uint8_t lower=0,upper;
    if (selectedField==0) { field=&editedDate.Date; lower=1; upper=monthLength(editedDate.Year,editedDate.Month); }
    else if (selectedField==1) { field=&editedDate.Month; lower=1; upper=12; }
    else if (selectedField==2) { field=&editedDate.Year; lower=24; upper=99; }
    else if (selectedField==3) { field=&editedTime.Hours; upper=23; }
    else if (selectedField==4) { field=&editedTime.Minutes; upper=59; }
    else { field=&editedTime.Seconds; upper=59; }
    if (direction>0) *field=*field>=upper?lower:*field+1;
    else *field=*field<=lower?upper:*field-1;
    uint8_t limit=monthLength(editedDate.Year,editedDate.Month);
    if (editedDate.Date>limit) editedDate.Date=limit;
    saveError=0; Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
}
void DTS_Increment(void) { change(1); }
void DTS_Decrement(void) { change(-1); }
void DTS_Next(void) { selectedField=(selectedField+1)%6; saveError=0; Main_SetBuzzerMode(BUZZER_SHORT_BEEP); }
void DTS_Cancel(void) { Main_SetBuzzerMode(BUZZER_SHORT_BEEP); back(); }
void DTS_Save(void)
{
    saveError=Rtc_Set(getUnixTimestamp(&editedTime,&editedDate));
    if (saveError) { Main_SetBuzzerMode(BUZZER_LONG_BEEP); return; }
    Main_SetBuzzerMode(BUZZER_2_SHORT_BEEP); back();
}
Screen_t dateTimeScreen={0,DTS_Init,DTS_Render,DTS_Increment,DTS_Decrement,DTS_Next,
    nothing,nothing,nothing,DTS_Cancel,DTS_Save,nothing,nothing};
