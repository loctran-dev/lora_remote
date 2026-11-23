#include "screens.h"
#include "remote_history.h"
#include "main.h"
#include "define.h"
#include "utility.h"
#include <stdio.h>
#include <string.h>
extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];
uint8_t historyScreenMode; /* 0 device picker, 1 frozen list, 2 details. */
static uint8_t chosenDevice;
static uint16_t selected,count,slots[HJ_SLOTS];
static uint32_t snapshotGeneration[HJ_PAGES],snapshotRevision;
static UG_TEXTBOX *boxes[]={&textbox_1,&textbox_2,&textbox_3,&textbox_4,&textbox_5,
    &textbox_6,&textbox_7,&textbox_8,&textbox_9,&textbox_10,&textbox_11,&textbox_12,
    &textbox_13,&textbox_14,&textbox_15,&textbox_16};
static void HS_Init(void);
static void HS_Render(void);
static void callback(UG_MESSAGE *message) { (void)message; }
static const HistoryRecord *raw(uint16_t slot)
{
    return (const HistoryRecord *)(HJ_BASE+(slot/HJ_PER_PAGE)*HJ_PAGE_SIZE+
        HJ_HEADER_SIZE+(slot%HJ_PER_PAGE)*32u);
}
static uint32_t pageGeneration(uint16_t p) { return *(const uint32_t *)(HJ_BASE+p*HJ_PAGE_SIZE); }
static uint8_t available(uint8_t id)
{
    if (!deviceBuff[id].isDelete) return 1;
    uint16_t owner=RemoteHistory_Owner(id);
    for (uint16_t s=0;s<HJ_SLOTS;s++) { const HistoryRecord *r=HJ_Get(s); if (r && r->owner==owner) return 1; }
    return 0;
}
static const HistoryRecord *view(uint16_t index)
{
    if (index>=count || snapshotGeneration[slots[index]/HJ_PER_PAGE]!=pageGeneration(slots[index]/HJ_PER_PAGE)) return 0;
    return HJ_Get(slots[index]);
}
static void snapshot(void)
{
    count=selected=0; snapshotRevision=remoteHistoryRevision;
    for (uint16_t p=0;p<HJ_PAGES;p++) snapshotGeneration[p]=pageGeneration(p);
    uint16_t owner=RemoteHistory_Owner(chosenDevice);
    for (uint16_t s=0;s<HJ_SLOTS;s++) {
        const HistoryRecord *r=HJ_Get(s);
        if (!r || r->owner!=owner || r->event.serial!=deviceBuff[chosenDevice].factoryInfo.serial) continue;
        uint16_t i=count++;
        /* All records were validated once. Sorting only reads immutable flash in uiMutex. */
        while (i) {
            const HistoryRecord *previous=raw(slots[i-1]);
            uint8_t newer;
            if (!((previous->event.flags|r->event.flags)&HIST_TIME_UNKNOWN) &&
                r->event.timestamp!=previous->event.timestamp)
                newer=r->event.timestamp>previous->event.timestamp;
            else {
                uint32_t a=snapshotGeneration[s/HJ_PER_PAGE],b=snapshotGeneration[slots[i-1]/HJ_PER_PAGE];
                newer=a>b || (a==b && s>slots[i-1]);
            }
            if (!newer) break;
            slots[i]=slots[i-1]; i--;
        }
        slots[i]=s;
    }
}
static char *powerName(uint8_t state) { return state==MOTOR_POWER_ON?"ON":state==MOTOR_POWER_OFF?"OFF":"?"; }
static char *actionName(uint8_t action)
{
    switch(action) {
        case HIST_ON:return "ON"; case HIST_OFF:return "OFF"; case HIST_ALARM:return "ALARM";
        case HIST_CLEAR:return "CLEAR"; case HIST_LINK_LOST:return "NO LINK";
        case HIST_LINK_RESTORED:return "LINK OK"; default:return "?";
    }
}
static char *sourceName(uint8_t source)
{
    switch(source) {
        case HIST_REMOTE:return "Remote"; case HIST_IO1:return "Slave IO1"; case HIST_IO2:return "Slave IO2";
        case HIST_IO3:return "Slave IO3"; case HIST_IO4:return "Slave IO4"; case HIST_TIMER:return "Timer";
        case HIST_MONITOR:return "Remote check"; default:return "Slave";
    }
}
static char *errorName(uint16_t error)
{
    return error==HIST_ERROR_LOW_CURRENT?"Low current":error==HIST_ERROR_NO_STATUS?"No STATUS":error?"Unknown":"None";
}
static char *resultName(uint8_t result)
{
    return result==HIST_APPLIED?"Applied":result==HIST_UNCHANGED?"No change":
        result==HIST_REJECTED?"Rejected":"Warning";
}
static void textbox(uint8_t id,int x,int y,int xe,int ye,char *text,UG_COLOR bg)
{
    UG_TextboxCreate(&window_1,boxes[id],id,x,y,xe,ye);
    UG_TextboxSetFont(&window_1,id,DEFAULT_FONT); UG_TextboxSetText(&window_1,id,text);
    UG_TextboxSetBackColor(&window_1,id,bg); UG_TextboxSetForeColor(&window_1,id,C_WHITE);
    UG_TextboxSetAlignment(&window_1,id,ALIGN_CENTER_LEFT);
    UG_TextboxSetScrollEnabled(&window_1,id,0);
}
static void show(uint8_t id,uint8_t visible)
{
    if (visible && !UG_TextboxCheckVisible(&window_1,id)) UG_TextboxShow(&window_1,id);
    if (!visible && UG_TextboxCheckVisible(&window_1,id)) UG_TextboxHide(&window_1,id);
}
static void rebuild(void)
{
    UG_WindowHide(&window_1); UG_WindowDelete(&window_1); HS_Init(); HS_Render();
}
static void HS_Init(void)
{
    UG_WindowCreate(&window_1,obj_buff_wnd_1,MAX_OBJECTS,callback);
    UG_WindowSetStyle(&window_1,WND_STYLE_HIDE_TITLE); UG_WindowSetTitleHeight(&window_1,0);
    UG_WindowSetBackColor(&window_1,C_BLACK);
    UG_WindowSetXStart(&window_1,0); UG_WindowSetYStart(&window_1,0);
    UG_WindowSetXEnd(&window_1,127); UG_WindowSetYEnd(&window_1,159);
    Screen_StatusBarCreate();
    textbox(2,0,144,35,159,historyScreenMode==2?"Prev":"Up",C_DARK_SLATE_BLUE);
    textbox(3,36,144,91,159,historyScreenMode==0?"Open":historyScreenMode==1?"Detail":"List",C_DARK_CYAN);
    textbox(4,92,144,127,159,historyScreenMode==2?"Next":"Down",C_DARK_SLATE_BLUE);
    for (uint8_t id=2;id<=4;id++) UG_TextboxSetAlignment(&window_1,id,ALIGN_CENTER);
    textbox(5,0,20,77,37,"",C_DARK_SLATE_BLUE); textbox(6,78,20,127,37,"",C_DARK_SLATE_BLUE);
    UG_TextboxSetAlignment(&window_1,6,ALIGN_CENTER_RIGHT);
    for (uint8_t i=0;i<8;i++) {
        int y=historyScreenMode==2?43+i*13:43+(i/2)*22+(i%2)*10;
        textbox(7+i,4,y,123,y+9,"",C_BLACK);
    }
    textbox(15,0,132,127,141,"Hold RIGHT: Back",C_BLACK);
    UG_TextboxSetAlignment(&window_1,15,ALIGN_CENTER);
    UG_WindowShow(&window_1);
}
static void HS_Render(void)
{
    char text[64];
    Screen_StatusBarUpdate();
    snprintf(text,sizeof(text),historyScreenMode==0?"Select device":"M%u1 %s",chosenDevice+1,
        historyScreenMode==1?"History":"Event"); UG_TextboxSetText(&window_1,5,text);
    if (!historyScreenMode) {
        uint8_t page=chosenDevice/4*4;
        snprintf(text,sizeof(text),"%u/5",page/4+1); UG_TextboxSetText(&window_1,6,text);
        for (uint8_t row=0;row<4;row++) {
            uint8_t id=page+row,a=7+row*2,b=a+1;
            uint8_t exists=id<MAXIMUM_DEVICE && available(id);
            show(a,exists); show(b,exists); if (!exists) continue;
            snprintf(text,sizeof(text),"M%u1%s",id+1,deviceBuff[id].motorData[0].isHide?" [Hidden]":"");
            UG_TextboxSetText(&window_1,a,text);
            UG_TextboxSetText(&window_1,b,deviceBuff[id].isDelete?"Removed device":deviceBuff[id].motorData[0].name);
            for (uint8_t cell=a;cell<=b;cell++) {
                UG_TextboxSetBackColor(&window_1,cell,id==chosenDevice?C_DIM_GRAY:C_BLACK);
                UG_TextboxSetForeColor(&window_1,cell,cell==b?C_CYAN:C_WHITE);
                UG_TextboxSetScrollEnabled(&window_1,cell,id==chosenDevice);
            }
        }
    } else if (historyScreenMode==1) {
        snprintf(text,sizeof(text),"%u/%u",count?selected/4+1:0,(count+3)/4); UG_TextboxSetText(&window_1,6,text);
        for (uint8_t row=0;row<4;row++) {
            uint16_t index=selected/4*4+row; uint8_t a=7+row*2,b=a+1;
            const HistoryRecord *r=view(index);
            show(a,index<count || (!count && !row)); show(b,index<count);
            if (!r) {
                if (index<count) { UG_TextboxSetText(&window_1,a,"Record expired"); UG_TextboxSetText(&window_1,b,"Hold CENTER: Refresh"); }
                else if (!count && !row) UG_TextboxSetText(&window_1,a,"No history yet");
                continue;
            }
            RTC_TimeTypeDef time; RTC_DateTypeDef date;
            convertTimestampToRTC(r->event.timestamp,&time,&date);
            if (r->event.flags&HIST_TIME_UNKNOWN) snprintf(text,sizeof(text),"--:--:-- %s",actionName(r->event.action));
            else snprintf(text,sizeof(text),"%02u:%02u:%02u %s",time.Hours,time.Minutes,time.Seconds,actionName(r->event.action));
            UG_TextboxSetText(&window_1,a,text);
            snprintf(text,sizeof(text),"%s %s > %s",r->event.source==HIST_MONITOR?"Remote":sourceName(r->event.source),
                powerName(r->event.before),powerName(r->event.after));
            UG_TextboxSetText(&window_1,b,text);
            for (uint8_t cell=a;cell<=b;cell++) {
                UG_TextboxSetBackColor(&window_1,cell,index==selected?C_DIM_GRAY:C_BLACK);
                UG_TextboxSetForeColor(&window_1,cell,cell==b?C_CYAN:r->event.error?C_ORANGE:C_WHITE);
                UG_TextboxSetScrollEnabled(&window_1,cell,index==selected);
            }
        }
    } else {
        snprintf(text,sizeof(text),"%u/%u",count?selected+1:0,count); UG_TextboxSetText(&window_1,6,text);
        const HistoryRecord *r=view(selected);
        for (uint8_t i=0;i<8;i++) { show(7+i,i<7); UG_TextboxSetBackColor(&window_1,7+i,C_BLACK); UG_TextboxSetScrollEnabled(&window_1,7+i,0); }
        if (!r) {
            UG_TextboxSetText(&window_1,7,"Record expired");
            for (uint8_t i=1;i<7;i++) UG_TextboxSetText(&window_1,7+i,"");
        } else {
            RTC_TimeTypeDef time; RTC_DateTypeDef date;
            convertTimestampToRTC(r->event.timestamp,&time,&date);
            if (r->event.flags&HIST_TIME_UNKNOWN) strcpy(text,"Time not synced");
            else snprintf(text,sizeof(text),"%02u/%02u/%02u %02u:%02u:%02u",date.Date,date.Month,date.Year,time.Hours,time.Minutes,time.Seconds);
            UG_TextboxSetText(&window_1,7,text);
            char *labels[]={"Action","Source","Before","After","Result","Error"};
            char *values[]={actionName(r->event.action),sourceName(r->event.source),
                r->event.before?powerName(r->event.before):"Unknown",
                r->event.after?powerName(r->event.after):"Unknown",resultName(r->event.result),errorName(r->event.error)};
            for (uint8_t i=0;i<6;i++) {
                snprintf(text,sizeof(text),"%s: %s",labels[i],values[i]);
                UG_TextboxSetText(&window_1,8+i,text);
                UG_TextboxSetForeColor(&window_1,8+i,i==5 && r->event.error?C_ORANGE:C_CYAN);
            }
        }
    }
    UG_TextboxSetText(&window_1,15,historyScreenMode && snapshotRevision!=remoteHistoryRevision?
        "New: Hold CTR refresh":"Hold RIGHT: Back");
    UG_Update();
}
static void move(int8_t direction)
{
    if (!historyScreenMode) {
        int id=chosenDevice+direction;
        for (;id>=0 && id<MAXIMUM_DEVICE;id+=direction) if (available(id)) { chosenDevice=id; break; }
    } else if (count) {
        if (direction<0 && selected) selected--;
        if (direction>0 && selected+1<count) selected++;
    }
    HS_Render();
}
static void HS_Left(void) { move(-1); }
static void HS_Right(void) { move(1); }
static void HS_Center(void)
{
    if (!historyScreenMode) { if (!available(chosenDevice)) return; snapshot(); historyScreenMode=1; }
    else if (historyScreenMode==1) { if (!count) return; historyScreenMode=2; }
    else historyScreenMode=1;
    rebuild();
}
static void HS_Back(void)
{
    if (historyScreenMode) { historyScreenMode--; rebuild(); }
    else {
        UG_WindowHide(&window_1); UG_WindowDelete(&window_1);
        currentScreen=&mainMenuScreen; currentScreen->init(); currentScreen->render();
    }
}
static void HS_Refresh(void)
{
    if (!historyScreenMode) return;
    uint16_t oldSlot=count?slots[selected]:0xFFFF;
    uint32_t oldGeneration=count?snapshotGeneration[oldSlot/HJ_PER_PAGE]:0;
    snapshot();
    for (uint16_t i=0;i<count;i++) if (slots[i]==oldSlot && snapshotGeneration[oldSlot/HJ_PER_PAGE]==oldGeneration) { selected=i; break; }
    HS_Render();
}
static void noop(void) { }
void Screen_OpenHistory(void)
{
    chosenDevice=systemInfo.currentDeviceId<MAXIMUM_DEVICE?systemInfo.currentDeviceId:0;
    if (!available(chosenDevice)) for (uint8_t id=0;id<MAXIMUM_DEVICE;id++) if (available(id)) { chosenDevice=id; break; }
    historyScreenMode=0;
    UG_WindowHide(&window_1); UG_WindowDelete(&window_1);
    currentScreen=&historyScreen; HS_Init(); HS_Render();
}
Screen_t historyScreen={0,HS_Init,HS_Render,HS_Left,HS_Right,HS_Center,noop,noop,noop,HS_Back,HS_Refresh,noop,noop};
