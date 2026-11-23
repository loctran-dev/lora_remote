#include "link_monitor.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"
#include "cmsis_os.h"
extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

static void window_1_callback(UG_MESSAGE *msg)
{
}
void DMS_LeftButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
}
void DMS_LeftButtonPressedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
}
void DMS_RightButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	
	UG_WindowHide(&window_1);
  UG_WindowDelete(&window_1);
	currentScreen = &deviceControlScreen;
	currentScreen->init();
	currentScreen->render();
}
void DMS_RightButtonPressedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	
	UG_WindowHide(&window_1);
  UG_WindowDelete(&window_1);
	currentScreen = &deviceControlScreen;
	currentScreen->init();
	currentScreen->render();
}
void DMS_CenterButtonClickedCallback(void)
{
}
void DMS_CenterButtonPressedCallback(void)
{
}
void DMS_BLeftButtonClickedCallback(void)
{
}
void DMS_BLeftButtonPressedCallback(void)
{
}
void DMS_BRightButtonClickedCallback(void)
{
}
void DMS_BRightButtonPressedCallback(void)
{
}
static void monitorText(UG_TEXTBOX *box,uint8_t id,int x,int y,int xe,int ye,char *text)
{
    UG_TextboxCreate(&window_1,box,id,x,y,xe,ye);
    UG_TextboxSetFont(&window_1,id,DEFAULT_FONT);
    UG_TextboxSetText(&window_1,id,text);
    UG_TextboxSetBackColor(&window_1,id,C_BLACK);
    UG_TextboxSetForeColor(&window_1,id,C_WHITE);
    UG_TextboxSetAlignment(&window_1,id,ALIGN_CENTER_LEFT);
    UG_TextboxSetScrollEnabled(&window_1,id,0);
}
void DMS_Init(void)
{
    UG_TEXTBOX *labels[]={&textbox_7,&textbox_9,&textbox_11,&textbox_13,&textbox_15,&textbox_17,&textbox_19};
    UG_TEXTBOX *values[]={&textbox_8,&textbox_10,&textbox_12,&textbox_14,&textbox_16,&textbox_18,&textbox_20};
    char *names[]={"Serial","Node","Type","Modules","Module","RSSI","Last RX"};
    UG_WindowCreate(&window_1,obj_buff_wnd_1,MAX_OBJECTS,window_1_callback);
    UG_WindowSetStyle(&window_1,WND_STYLE_HIDE_TITLE); UG_WindowSetTitleHeight(&window_1,0);
    UG_WindowSetBackColor(&window_1,C_BLACK);
    UG_WindowSetXStart(&window_1,0); UG_WindowSetYStart(&window_1,0);
    UG_WindowSetXEnd(&window_1,127); UG_WindowSetYEnd(&window_1,159);
    Screen_StatusBarCreate();
    monitorText(&textbox_6,5,0,20,127,37,"Device info");
    UG_TextboxSetBackColor(&window_1,5,C_DARK_SLATE_BLUE);
    UG_TextboxSetScrollEnabled(&window_1,5,1);
    for (uint8_t row=0;row<7;row++) {
        int y=42+row*14;
        monitorText(labels[row],6+row*2,3,y,55,y+11,names[row]);
        monitorText(values[row],7+row*2,57,y,124,y+11,"--");
        UG_TextboxSetAlignment(&window_1,7+row*2,ALIGN_CENTER_RIGHT);
        UG_TextboxSetForeColor(&window_1,7+row*2,C_CYAN);
    }
    monitorText(&textbox_3,2,0,144,35,159,"");
    monitorText(&textbox_4,3,36,144,91,159,"");
    monitorText(&textbox_5,4,92,144,127,159,"Back");
    for (uint8_t id=2;id<=4;id++) UG_TextboxSetBackColor(&window_1,id,C_RED);
    UG_TextboxSetAlignment(&window_1,4,ALIGN_CENTER_RIGHT);
    UG_WindowShow(&window_1);
}
void DMS_Render(void)
{
    char text[40];
    uint8_t id=systemInfo.currentDeviceId;
    Screen_StatusBarUpdate();
    if (id>=MAXIMUM_DEVICE || deviceBuff[id].isDelete) {
        UG_TextboxSetText(&window_1,5,"No paired device");
        for (uint8_t row=0;row<7;row++) UG_TextboxSetText(&window_1,7+row*2,"--");
        UG_Update(); return;
    }
    DeviceInfo_t *device=&deviceBuff[id];
    uint8_t module=systemInfo.currentModuleId;
    if (module>=sizeof(device->motorData)/sizeof(device->motorData[0])) module=0;
    snprintf(text,sizeof(text),"%c%u%u Info: %.19s",device->factoryInfo.deviceTypeChar,
        device->deviceIndex+1,module+1,device->motorData[module].name);
    UG_TextboxSetText(&window_1,5,text);
    snprintf(text,sizeof(text),"0x%08lX",(unsigned long)device->factoryInfo.serial);
    UG_TextboxSetText(&window_1,7,text);
    snprintf(text,sizeof(text),"0x%02X",device->nodeId); UG_TextboxSetText(&window_1,9,text);
    snprintf(text,sizeof(text),"%c",device->factoryInfo.deviceTypeChar); UG_TextboxSetText(&window_1,11,text);
    snprintf(text,sizeof(text),"%u",device->factoryInfo.numberOfModule); UG_TextboxSetText(&window_1,13,text);
    snprintf(text,sizeof(text),"%u",module+1); UG_TextboxSetText(&window_1,15,text);
    snprintf(text,sizeof(text),"%d dBm",device->rssi); UG_TextboxSetText(&window_1,17,text);
    /* RTC can be edited; the link supervisor's monotonic age cannot underflow. */
    uint32_t seconds=Link_Now()-linkDevices[id].lastRx;
    if (seconds<100) snprintf(text,sizeof(text),"%lu s",(unsigned long)seconds);
    else if (seconds/60<100) snprintf(text,sizeof(text),"%lu min",(unsigned long)(seconds/60));
    else if (seconds/3600<100) snprintf(text,sizeof(text),"%lu h",(unsigned long)(seconds/3600));
    else snprintf(text,sizeof(text),"%lu d",(unsigned long)(seconds/86400));
    UG_TextboxSetText(&window_1,19,text);
    UG_Update();
}
Screen_t deviceMonitorScreen = 
{ 
	0,
	DMS_Init,
	DMS_Render,
	DMS_LeftButtonClickedCallback,
	DMS_RightButtonClickedCallback,
	DMS_CenterButtonClickedCallback,
	DMS_BLeftButtonClickedCallback,
	DMS_BRightButtonClickedCallback,
	DMS_LeftButtonPressedCallback,
	DMS_RightButtonPressedCallback,
	DMS_CenterButtonPressedCallback,
	DMS_BLeftButtonPressedCallback,
	DMS_BRightButtonPressedCallback
};
