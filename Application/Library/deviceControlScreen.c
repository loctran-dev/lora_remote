#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "image_data.h"
#include "screens.h"
#include "main.h"
#include "utility.h"
#include "powerSleep.h"
#include "schedule.h"
extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

uint8_t inScheduleSelection = 0;
uint8_t indexScheduleSelection = 0;
uint8_t indexScheduleValue = 0;
uint8_t adjustingScheduleValue = 0;
static UG_OBJECT* statusIconsObject;
static uint32_t statusIconsKey;
static DeviceInfo_t statusIconDevice;
static MotorData_t statusIconModule;
static volatile uint8_t lockNoticeActive;
static volatile uint8_t lockNoticePower;
static volatile uint32_t lockNoticeTick;
static int8_t connectionNotice = -1;
static uint32_t connectionNoticeTick;

void Screen_ShowConnectionError(uint8_t id)
{
    Power_WakeForError();
    Main_SetBuzzerMode(BUZZER_3_SHORT_BEEP);
    if (Screen_DeviceVisible(id) && currentScreen != &editDeviceSettingScreen &&
        currentScreen != &editGeneralSettingScreen && currentScreen != &dateTimeScreen &&
        currentScreen != &historyScreen && !inScheduleSelection) {
        UG_WindowHide(&window_1); UG_WindowDelete(&window_1);
        systemInfo.currentDeviceId = id;
        systemInfo.currentModuleId = 0;
        currentScreen = &deviceControlScreen;
        currentScreen->init();
    }
    connectionNotice = id;
    connectionNoticeTick = HAL_GetTick();
}

void Screen_ShowMotorLocked(uint8_t motorPower)
{
	lockNoticePower = motorPower;
	lockNoticeTick = HAL_GetTick();
	lockNoticeActive = 1;
}

static void DCS_IconText(char* text, int16_t xs, int16_t ys,
                         int16_t xe, int16_t ye, UG_COLOR color, UG_COLOR background)
{
	UG_TEXT label = {0};
	label.a.xs = xs; label.a.ys = ys;
	label.a.xe = xe; label.a.ye = ye;
	label.font = DEFAULT_FONT;
	label.str = text;
	label.align = ALIGN_CENTER;
	label.fc = color; label.bc = background;
	_UG_PutText(&label);
}

static void DCS_DrawStatusIcons(const DeviceInfo_t* device, const MotorData_t* module)
{
	const UG_COLOR background = 0x0841;
	UG_COLOR color = C_ORANGE;
	char* status = "UNKNOWN";
	uint8_t known = !device->isDelete && !device->isConnectionError &&
		(module->pwrState == MOTOR_POWER_ON || module->pwrState == MOTOR_POWER_OFF);
	if (device->isDelete || device->isConnectionError) status = "NO LINK";
	else if (module->isAlarmOccurred) { status = "ALARM"; color = C_RED; }
	else if (known)
	{
		status = module->pwrState == MOTOR_POWER_ON ? "ON" : "OFF";
		color = module->pwrState == MOTOR_POWER_ON ? C_GREEN : C_RED;
	}
	UG_FillFrame(9, 44, 69, 88, C_BLACK);
	UG_FillRoundFrame(10, 45, 68, 87, 6, background);
	UG_DrawRoundFrame(10, 45, 68, 87, 6, color);
	if (known && !module->isAlarmOccurred)
	{
		UG_DrawCircle(39, 60, 10, color);
		UG_DrawCircle(39, 60, 9, color);
		UG_FillFrame(35, 48, 43, 55, background);
		UG_FillFrame(38, 48, 40, 60, color);
	}
	else
	{
		UG_DrawLine(39, 48, 27, 68, color);
		UG_DrawLine(27, 68, 51, 68, color);
		UG_DrawLine(51, 68, 39, 48, color);
		UG_FillFrame(38, 55, 40, 61, color);
		UG_FillFrame(38, 64, 40, 65, color);
	}
	DCS_IconText(status, 12, 74, 66, 84, color, background);
	DCS_IconText("A", 71, 47, 78, 57, C_CYAN, C_BLACK);
	DCS_IconText("B", 71, 61, 78, 71, C_CYAN, C_BLACK);
	DCS_IconText("C", 71, 75, 78, 85, C_CYAN, C_BLACK);
}

static void DCS_StatusIconsUpdate(UG_WINDOW* window, UG_OBJECT* object)
{
	(void)window;
	if ((object->state & OBJ_STATE_UPDATE) && (object->state & OBJ_STATE_REDRAW))
		DCS_DrawStatusIcons(&statusIconDevice, &statusIconModule);
	object->state &= ~(OBJ_STATE_UPDATE | OBJ_STATE_REDRAW);
}

static void DCS_SetStatusIcons(const DeviceInfo_t* device, const MotorData_t* module)
{
	uint32_t key = (device->isDelete ? 1u : 0u) |
		(device->isConnectionError ? 2u : 0u) |
		(module->isAlarmOccurred ? 4u : 0u) |
		((uint32_t)module->pwrState << 8) |
		(module->scheduleTime > 0 ? 0x10000u : 0u);
	if (statusIconsObject && key != statusIconsKey)
	{
		statusIconsKey = key;
		statusIconDevice = *device;
		statusIconModule = *module;
		statusIconsObject->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
	}
}

static void window_1_callback(UG_MESSAGE *msg)
{
}

/* Timer UI uses numeric state, not cursor positions in a formatted string. */
static uint16_t scheduleCustomMinutes;
static uint8_t timerZeroError;
uint8_t Screen_IsScheduleEditing(void) { return inScheduleSelection != 0; }

static void DCS_TimerText(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye,
                          char *text, UG_FONT *font, UG_COLOR background)
{
    UG_TextboxCreate(&window_1, box, id, x, y, xe, ye);
    UG_TextboxSetFont(&window_1, id, font);
    UG_TextboxSetText(&window_1, id, text);
    UG_TextboxSetBackColor(&window_1, id, background);
    UG_TextboxSetForeColor(&window_1, id, C_WHITE);
    UG_TextboxSetAlignment(&window_1, id, ALIGN_CENTER);
    UG_TextboxSetScrollEnabled(&window_1, id, 0);
}

static void DCS_TimerRender(void)
{
    char text[32];
    Screen_StatusBarUpdate();
    if (adjustingScheduleValue) {
        uint8_t digits[] = {scheduleCustomMinutes / 60 / 10, scheduleCustomMinutes / 60 % 10,
                            scheduleCustomMinutes % 60 / 10, scheduleCustomMinutes % 10};
        for (uint8_t i = 0; i < 4; i++) {
            char digit[] = {'0' + digits[i], 0};
            UG_TextboxSetText(&window_1, 32 + i, digit);
            UG_TextboxSetBackColor(&window_1, 32 + i, i == indexScheduleValue ? C_YELLOW : 0x2104);
            UG_TextboxSetForeColor(&window_1, 32 + i, i == indexScheduleValue ? C_BLACK : C_WHITE);
        }
        UG_TextboxSetText(&window_1, 39, timerZeroError ? "Minimum: 1 minute" : "Hold CENTER: Apply");
        UG_TextboxSetForeColor(&window_1, 39, timerZeroError ? C_ORANGE : C_WHITE);
    } else {
        uint8_t page = indexScheduleSelection / 4 * 4;
        snprintf(text, sizeof(text), "%u/26", page / 4 + 1);
        UG_TextboxSetText(&window_1, TXB_ID_10, text);
        for (uint8_t row = 0; row < 4; row++) {
            uint8_t item = page + row, id = 11 + row;
            if (item > 100) {
                if (UG_TextboxCheckVisible(&window_1, id)) UG_TextboxHide(&window_1, id);
                continue;
            }
            if (!UG_TextboxCheckVisible(&window_1, id)) UG_TextboxShow(&window_1, id);
            if (!item) strcpy(text, "Custom time");
            else if (item == 1) strcpy(text, "Disable timer");
            else {
                uint16_t minutes = (item - 1) * 30;
                snprintf(text, sizeof(text), "%02u h %02u min", minutes / 60, minutes % 60);
            }
            UG_TextboxSetText(&window_1, id, text);
            UG_TextboxSetBackColor(&window_1, id, item == indexScheduleSelection ? C_DIM_GRAY : C_BLACK);
            UG_TextboxSetForeColor(&window_1, id, item == indexScheduleSelection ? C_CYAN : C_WHITE);
        }
    }
    UG_Update();
}

static void DCS_TimerScreenCreate(void)
{
    UG_WindowCreate(&window_1, obj_buff_wnd_1, MAX_OBJECTS, window_1_callback);
    UG_WindowSetStyle(&window_1, WND_STYLE_HIDE_TITLE);
    UG_WindowSetTitleHeight(&window_1, 0);
    UG_WindowSetBackColor(&window_1, C_BLACK);
    UG_WindowSetXStart(&window_1, 0); UG_WindowSetYStart(&window_1, 0);
    UG_WindowSetXEnd(&window_1, 127); UG_WindowSetYEnd(&window_1, 159);
    Screen_StatusBarCreate();
    char title[24];
    DeviceInfo_t *device = &deviceBuff[systemInfo.currentDeviceId];
    char type = device->factoryInfo.deviceTypeChar;
    if (type < 32 || type > 126) type = 'M';
    snprintf(title, sizeof(title), "%c%u%u Timer", type, device->deviceIndex + 1,
             systemInfo.currentModuleId + 1);
    DCS_TimerText(&textbox_10, 9, 0, 20, 80, 37, title, DEFAULT_FONT, C_DARK_SLATE_BLUE);
    DCS_TimerText(&textbox_11, 10, 81, 20, 127, 37,
                 adjustingScheduleValue ? "HH:MM" : "", DEFAULT_FONT, C_DARK_SLATE_BLUE);
    DCS_TimerText(&textbox_3, 2, 0, 144, 35, 159, adjustingScheduleValue ? "+" : "Up",
                 DEFAULT_FONT, C_DARK_SLATE_BLUE);
    DCS_TimerText(&textbox_4, 3, 36, 144, 91, 159, adjustingScheduleValue ? "Next" : "Select",
                 DEFAULT_FONT, C_DARK_CYAN);
    DCS_TimerText(&textbox_5, 4, 92, 144, 127, 159, adjustingScheduleValue ? "-" : "Down",
                 DEFAULT_FONT, C_DARK_SLATE_BLUE);
    UG_TEXTBOX *boxes[] = {&textbox_12, &textbox_13, &textbox_14, &textbox_15};
    if (adjustingScheduleValue) {
        DCS_TimerText(&textbox_6, 11, 0, 43, 127, 55, "Custom duration", DEFAULT_FONT, C_BLACK);
        for (uint8_t i = 0; i < 4; i++) {
            int x = 14 + i * 22 + (i >= 2 ? 14 : 0);
            DCS_TimerText(boxes[i], 32 + i, x, 65, x + 19, 94, "0", FONT_12X20, 0x2104);
        }
        DCS_TimerText(&textbox_16, 36, 58, 65, 69, 94, ":", FONT_12X20, C_BLACK);
        DCS_TimerText(&textbox_17, 37, 14, 98, 55, 109, "Hours", DEFAULT_FONT, C_BLACK);
        DCS_TimerText(&textbox_18, 38, 72, 98, 113, 109, "Minutes", DEFAULT_FONT, C_BLACK);
        DCS_TimerText(&textbox_19, 39, 0, 115, 127, 125, "", DEFAULT_FONT, C_BLACK);
        DCS_TimerText(&textbox_20, 40, 0, 130, 127, 140, "Hold RIGHT: Cancel", DEFAULT_FONT, C_BLACK);
    } else {
        for (uint8_t row = 0; row < 4; row++)
            DCS_TimerText(boxes[row], 11 + row, 4, 43 + row * 22, 123, 62 + row * 22,
                          "", DEFAULT_FONT, C_BLACK);
        DCS_TimerText(&textbox_19, 39, 0, 132, 127, 141, "Hold RIGHT: Back", DEFAULT_FONT, C_BLACK);
    }
    UG_WindowShow(&window_1);
    DCS_TimerRender();
}

static void DCS_AdjustScheduleDigit(uint8_t increase)
{
    uint8_t digits[] = {scheduleCustomMinutes / 60 / 10, scheduleCustomMinutes / 60 % 10,
                        scheduleCustomMinutes % 60 / 10, scheduleCustomMinutes % 10};
    if (indexScheduleValue > 3) indexScheduleValue = 0;
    uint8_t radix = indexScheduleValue == 2 ? 6 : 10;
    digits[indexScheduleValue] = (digits[indexScheduleValue] + (increase ? 1 : radix - 1)) % radix;
    scheduleCustomMinutes = (digits[0] * 10 + digits[1]) * 60 + digits[2] * 10 + digits[3];
    timerZeroError = 0;
}

static void DCS_ReturnFromTimer(void)
{
    UG_WindowHide(&window_1); UG_WindowDelete(&window_1);
    inScheduleSelection = adjustingScheduleValue = 0;
    timerZeroError = 0;
    currentScreen = &deviceControlScreen;
    currentScreen->init();
    currentScreen->render();
}

static void DCS_FormatRemaining(uint32_t seconds, char *text)
{
    if (seconds > 359999u) seconds = 359999u; /* Maximum 99:59:59, no overflow. */
    snprintf(text, 9, "%02lu:%02lu:%02lu", (unsigned long)(seconds / 3600),
             (unsigned long)(seconds / 60 % 60), (unsigned long)(seconds % 60));
}

static void DCS_CountdownUpdate(const DeviceInfo_t *device, const MotorData_t *module)
{
    Schedule_State_e state = Schedule_GetStateForNode(device->nodeId);
    if (state == eSchedule_State_Waiting || state >= eSchedule_State_Rejected) {
        UG_TextboxSetText(&window_1, 11, "Timer");
        UG_TextboxSetText(&window_1, 12, state == eSchedule_State_Waiting ? "Sending" :
            state == eSchedule_State_Rejected ? "Rejected" : state == eSchedule_State_Timeout ? "No reply" : "Queue err");
        UG_TextboxSetForeColor(&window_1, 12, state == eSchedule_State_Waiting ? C_CYAN : C_ORANGE);
        if (UG_TextboxCheckVisible(&window_1, TXB_ID_8)) UG_TextboxHide(&window_1, TXB_ID_8);
        return;
    }
    UG_TextboxSetForeColor(&window_1, 12, C_WHITE);
    uint8_t present = module->pwrState == MOTOR_POWER_ON && module->scheduleTime != 0;
    UG_TextboxSetText(&window_1, 11, device->isConnectionError && present ? "Last timer" : "Auto stop");
    if (!present) {
        UG_TextboxSetText(&window_1, 12, "Not set");
        if (UG_TextboxCheckVisible(&window_1, TXB_ID_8)) UG_TextboxHide(&window_1, TXB_ID_8);
        return;
    }
    char text[16];
    uint32_t remaining = module->scheduleTime > systemInfo.currentTime ?
                         module->scheduleTime - systemInfo.currentTime : 0;
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    convertTimestampToRTC(module->scheduleTime, &time, &date);
    snprintf(text, sizeof(text), "At %02u:%02u", time.Hours, time.Minutes);
    UG_TextboxSetText(&window_1, 12, device->isConnectionError ? "Offline" : text);
    DCS_FormatRemaining(remaining, text);
    UG_TextboxSetText(&window_1, TXB_ID_8, text);
    UG_TextboxSetForeColor(&window_1, TXB_ID_8,
                          device->isConnectionError || !remaining ? C_ORANGE : C_CYAN);
    if (!UG_TextboxCheckVisible(&window_1, TXB_ID_8)) UG_TextboxShow(&window_1, TXB_ID_8);
}

void DCS_LeftButtonClickedCallback(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (!Screen_DeviceVisible(systemInfo.currentDeviceId)) return;
    if (!inScheduleSelection) Screen_SelectAdjacentVisible(-1);
    else if (adjustingScheduleValue) DCS_AdjustScheduleDigit(1);
    else if (indexScheduleSelection) indexScheduleSelection--;
    if (inScheduleSelection) DCS_TimerRender();
}
void DCS_LeftButtonPressedCallback(void)
{
    if (inScheduleSelection) return;
	if (!Screen_DeviceVisible(systemInfo.currentDeviceId)) return;
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	
	UG_WindowHide(&window_1);
  UG_WindowDelete(&window_1);
	currentScreen = &deviceMonitorScreen;
	currentScreen->init();
	currentScreen->render();
}
void DCS_RightButtonClickedCallback(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (!Screen_DeviceVisible(systemInfo.currentDeviceId)) return;
    if (!inScheduleSelection) Screen_SelectAdjacentVisible(1);
    else if (adjustingScheduleValue) DCS_AdjustScheduleDigit(0);
    else if (indexScheduleSelection < 100) indexScheduleSelection++;
    if (inScheduleSelection) DCS_TimerRender();
}
void DCS_RightButtonPressedCallback(void)
{
    if (inScheduleSelection) {
        Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
        DCS_ReturnFromTimer();
        return;
    }
	if (!Screen_DeviceVisible(systemInfo.currentDeviceId)) {
		Screen_OpenDeviceManager();
		return;
	}
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	
	UG_WindowHide(&window_1);
  UG_WindowDelete(&window_1);
	currentScreen = &deviceSettingScreen;
	Screen_DeviceSettingsResetSelection();
	currentScreen->init();
	currentScreen->render();
}
void DCS_CenterButtonClickedCallback(void)
{
    if (!Screen_DeviceVisible(systemInfo.currentDeviceId)) {
        Screen_OpenDeviceManager();
        return;
    }
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (!inScheduleSelection) {
        inScheduleSelection = 1;
        adjustingScheduleValue = indexScheduleValue = indexScheduleSelection = timerZeroError = 0;
        scheduleCustomMinutes = 0;
        UG_WindowHide(&window_1); UG_WindowDelete(&window_1);
        DCS_TimerScreenCreate();
    } else if (adjustingScheduleValue) {
        indexScheduleValue = (indexScheduleValue + 1) % 4;
        DCS_TimerRender();
    } else if (!indexScheduleSelection) {
        adjustingScheduleValue = 1;
        indexScheduleValue = timerZeroError = 0;
        UG_WindowHide(&window_1); UG_WindowDelete(&window_1);
        DCS_TimerScreenCreate();
    } else {
        int32_t seconds = indexScheduleSelection == 1 ? -1 : (indexScheduleSelection - 1) * 1800;
        Main_SetScheduleTimeToCurrentModule(seconds);
        DCS_ReturnFromTimer();
    }
}
void DCS_CenterButtonPressedCallback(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (!inScheduleSelection) {
        UG_WindowHide(&window_1); UG_WindowDelete(&window_1);
        currentScreen = &mainMenuScreen;
        currentScreen->init(); currentScreen->render();
    } else if (!adjustingScheduleValue) {
        DCS_ReturnFromTimer(); /* Back does not submit the highlighted preset. */
    } else if (!scheduleCustomMinutes) {
        timerZeroError = 1;
        Main_SetBuzzerMode(BUZZER_LONG_BEEP);
        DCS_TimerRender();
    } else {
        Main_SetScheduleTimeToCurrentModule((int32_t)scheduleCustomMinutes * 60);
        DCS_ReturnFromTimer();
    }
}
void DCS_BLeftButtonClickedCallback(void)
{
}
void DCS_BLeftButtonPressedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	Main_SetMotorPower(MOTOR_POWER_ON);
}
void DCS_BRightButtonClickedCallback(void)
{
}
void DCS_BRightButtonPressedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	Main_SetMotorPower(MOTOR_POWER_OFF);
}
void DCS_Init(void)
{
	lockNoticeActive = 0;
	if (!Screen_EnsureVisibleDevice()) {
		inScheduleSelection = 0;
		Screen_EmptyControlInit();
		return;
	}
	if (inScheduleSelection) { DCS_TimerScreenCreate(); return; }
	// Create the window
	UG_WindowCreate(&window_1, obj_buff_wnd_1, MAX_OBJECTS, window_1_callback);
	UG_WindowSetTitleText(&window_1, "");
	UG_WindowSetTitleTextFont(&window_1, DEFAULT_FONT);
	UG_WindowSetTitleHeight(&window_1, 0);
	UG_WindowSetStyle(&window_1, WND_STYLE_HIDE_TITLE);
	UG_WindowSetBackColor(&window_1, C_BLACK);
	UG_WindowSetXStart(&window_1, 0);
	UG_WindowSetYStart(&window_1, 0);
	UG_WindowSetXEnd(&window_1, LCD_WIDTH-1);
	UG_WindowSetYEnd(&window_1, LCD_HEIGHT-1);
	statusIconsObject = _UG_GetFreeObject(&window_1);
	statusIconsKey = 0xFFFFFFFFu;
	if (statusIconsObject)
	{
		statusIconsObject->type = 8;
		statusIconsObject->id = 240;
		statusIconsObject->update = DCS_StatusIconsUpdate;
		statusIconsObject->state = OBJ_STATE_VALID | OBJ_STATE_VISIBLE;
	}
	
	Screen_StatusBarCreate();
	
	/* Power status is drawn with crisp LCD primitives after widget updates. */
	
	// Create Textbox left button
	UG_TextboxCreate(&window_1, &textbox_3, TXB_ID_2, 0, 144, 35, 159);
	UG_TextboxSetFont(&window_1, TXB_ID_2, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_2, " Pre");
	UG_TextboxSetBackColor(&window_1, TXB_ID_2, C_RED);
	UG_TextboxSetForeColor(&window_1, TXB_ID_2, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_2, ALIGN_CENTER_LEFT);
	
	// Create Textbox center button
	UG_TextboxCreate(&window_1, &textbox_4, TXB_ID_3, 36, 144, 91, 159);
	UG_TextboxSetFont(&window_1, TXB_ID_3, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_3, "Set time");
	UG_TextboxSetBackColor(&window_1, TXB_ID_3, C_RED);
	UG_TextboxSetForeColor(&window_1, TXB_ID_3, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_3, ALIGN_CENTER);

	// Create Textbox rigth button
	UG_TextboxCreate(&window_1, &textbox_5, TXB_ID_4, 92, 144, 127, 159);
	UG_TextboxSetFont(&window_1, TXB_ID_4, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_4, "Next ");
	UG_TextboxSetBackColor(&window_1, TXB_ID_4, C_RED);
	UG_TextboxSetForeColor(&window_1, TXB_ID_4, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_4, ALIGN_CENTER_RIGHT);
	
	// Create Textbox 
	DeviceInfo_t device = deviceBuff[systemInfo.currentDeviceId];
	MotorData_t module = device.motorData[systemInfo.currentModuleId];
	
	char title[30];
	char currentAText[20];
	char currentBText[20];
	char currentCText[20];
	char rssiText[20];
	
	snprintf(title, sizeof(title), " %c%u%u.%.19s", device.factoryInfo.deviceTypeChar,device.deviceIndex + 1, module.moduleIndex + 1, module.name);
	sprintf(currentAText, "%5.2f A", module.phaseCurrent[0]);
	sprintf(currentBText, "%5.2f A", module.phaseCurrent[1]);
	sprintf(currentCText, "%5.2f A", module.phaseCurrent[2]);	
	sprintf(rssiText, "%ddbm  ", device.rssi);	
	UG_TextboxCreate(&window_1, &textbox_10, TXB_ID_9, 0, 20, 80, 37);
	UG_TextboxSetFont(&window_1, TXB_ID_9, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_9, title);
	UG_TextboxSetBackColor(&window_1, TXB_ID_9, C_DARK_SLATE_BLUE);
	UG_TextboxSetForeColor(&window_1, TXB_ID_9, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_9, ALIGN_CENTER_LEFT);
	
	// Create BMP
//	UG_ImageCreate(&window_1, &image_2, IMG_ID_1, 95, 24, 116, 37);
//	UG_ImageSetBMP(&window_1, IMG_ID_1, &bmp_signal_status_5);
	
	UG_TextboxCreate(&window_1, &textbox_11, TXB_ID_10, 80, 20, 127, 37);
	UG_TextboxSetFont(&window_1, TXB_ID_10, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_10, rssiText);
	UG_TextboxSetBackColor(&window_1, TXB_ID_10, C_DARK_SLATE_BLUE);
	UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_GREEN);
	UG_TextboxSetAlignment(&window_1, TXB_ID_10, ALIGN_CENTER_RIGHT);
	
	// Create Textbox 
	UG_TextboxCreate(&window_1, &textbox_6, TXB_ID_5, 80, 47, 123, 57);
	UG_TextboxSetFont(&window_1, TXB_ID_5, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_5, currentAText);
	UG_TextboxSetBackColor(&window_1, TXB_ID_5, C_BLACK);
	UG_TextboxSetForeColor(&window_1, TXB_ID_5, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_5, ALIGN_CENTER_RIGHT);
	
	// Create Textbox 
	UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 80, 61, 123, 71);
	UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_6, currentBText);
	UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
	UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_RIGHT);
	
	// Create Textbox
	UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, 80, 75, 123, 85);
	UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_7, currentCText);
	UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
	UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER_RIGHT);
	

    DCS_TimerText(&textbox_12, 11, 4, 94, 65, 104, "Auto stop", DEFAULT_FONT, C_BLACK);
    UG_TextboxSetAlignment(&window_1, 11, ALIGN_CENTER_LEFT);
    DCS_TimerText(&textbox_13, 12, 66, 94, 123, 104, "Not set", DEFAULT_FONT, C_BLACK);
    UG_TextboxSetAlignment(&window_1, 12, ALIGN_CENTER_RIGHT);
    DCS_TimerText(&textbox_9, TXB_ID_8, 4, 108, 123, 132, "", FONT_12X20, C_BLACK);
    UG_TextboxSetForeColor(&window_1, TXB_ID_8, C_CYAN);

	UG_WindowShow(&window_1);
	UG_Update();
}
void DCS_Render(void)
{
	if (!Screen_DeviceVisible(systemInfo.currentDeviceId)) {
		Screen_EmptyControlRender();
		return;
	}
    if (inScheduleSelection) { DCS_TimerRender(); return; }
	DeviceInfo_t device = deviceBuff[systemInfo.currentDeviceId];
	MotorData_t module = device.motorData[systemInfo.currentModuleId];
	
	char title[30];
	
	char currentAText[20];
	char currentBText[20];
	char currentCText[20];
	
	snprintf(title, sizeof(title), " %c%u%u.%.19s", device.factoryInfo.deviceTypeChar,device.deviceIndex + 1, module.moduleIndex + 1, module.name);
	sprintf(currentAText, "%5.2f A", module.phaseCurrent[0]);
	sprintf(currentBText, "%5.2f A", module.phaseCurrent[1]);
	sprintf(currentCText, "%5.2f A", module.phaseCurrent[2]);	

	if (lockNoticeActive && (!module.isLock ||
		(uint32_t)(HAL_GetTick() - lockNoticeTick) >= 3000u))
		lockNoticeActive = 0;
	UG_TextboxSetBackColor(&window_1, TXB_ID_9,
		lockNoticeActive ? C_RED : C_DARK_SLATE_BLUE);
	UG_TextboxSetText(&window_1, TXB_ID_9, lockNoticeActive ?
		(lockNoticePower == MOTOR_POWER_ON ? "ON BLOCKED" : "OFF BLOCKED") : title);
	//UG_ImageSetBMP(&window_1, IMG_ID_1, &bmp_signal_status_5);
	UG_TextboxSetText(&window_1, TXB_ID_5, currentAText);
	UG_TextboxSetText(&window_1, TXB_ID_6, currentBText);
	UG_TextboxSetText(&window_1, TXB_ID_7, currentCText);
	
    DCS_CountdownUpdate(&device, &module);

	char rssiText[20];

	if (device.isConnectionError == TRUE) 
	{
		sprintf(rssiText, "!Error  ");	
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_RED);
		UG_TextboxSetText(&window_1, TXB_ID_10, rssiText);
	}
	else if (device.rssi > RSSI_LEVEL_1) 
	{
		sprintf(rssiText, "%ddbm  ", device.rssi);	
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_TOMATO);
		UG_TextboxSetText(&window_1, TXB_ID_10, rssiText);
	}
	else if (device.rssi > RSSI_LEVEL_2)
	{
		sprintf(rssiText, "%ddbm  ", device.rssi);	
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_ORANGE);
		UG_TextboxSetText(&window_1, TXB_ID_10, rssiText);
	}
	else if (device.rssi > RSSI_LEVEL_3)
	{
		sprintf(rssiText, "%ddbm  ", device.rssi);	
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_YELLOW);
		UG_TextboxSetText(&window_1, TXB_ID_10, rssiText);
	}
	else if (device.rssi > RSSI_LEVEL_4)
	{
		sprintf(rssiText, "%ddbm  ", device.rssi);	
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_GREEN_YELLOW);
		UG_TextboxSetText(&window_1, TXB_ID_10, rssiText);
	}
	else if (device.rssi > RSSI_LEVEL_5)
	{
		sprintf(rssiText, "%ddbm  ", device.rssi);	
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_LIGHT_GREEN);
		UG_TextboxSetText(&window_1, TXB_ID_10, rssiText);
	}
	else 
	{
		sprintf(rssiText, "%ddbm  ", device.rssi);	
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_GREEN);
		UG_TextboxSetText(&window_1, TXB_ID_10, rssiText);
	}
		
	/* Apply after the RSSI update so the notification remains visible. */
    if (connectionNotice >= 0) {
        if (!deviceBuff[connectionNotice].isConnectionError ||
            (uint32_t)(HAL_GetTick() - connectionNoticeTick) >= 6000u) connectionNotice = -1;
        else {
            char lost[16];
            snprintf(lost, sizeof(lost), "M%u1", connectionNotice + 1);
            UG_TextboxSetBackColor(&window_1, TXB_ID_9, C_RED);
            UG_TextboxSetText(&window_1, TXB_ID_9, "NO STATUS");
            UG_TextboxSetText(&window_1, TXB_ID_10, lost);
        }
    }
	UG_TextboxSetBackColor(&window_1, TXB_ID_10,
		(lockNoticeActive || connectionNotice >= 0) ? C_RED : C_DARK_SLATE_BLUE);
	if (lockNoticeActive) {
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_WHITE);
		UG_TextboxSetText(&window_1, TXB_ID_10, "LOCKED");
	}
    if (connectionNotice >= 0) {
        char lost[16];
        snprintf(lost, sizeof(lost), "M%u1", connectionNotice + 1);
        UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_WHITE);
        UG_TextboxSetText(&window_1, TXB_ID_10, lost);
    }
	Screen_StatusBarUpdate();
	DCS_SetStatusIcons(&device, &module);
	UG_Update();
}
Screen_t deviceControlScreen = 
{ 
	0,
	DCS_Init,
	DCS_Render,
	DCS_LeftButtonClickedCallback,
	DCS_RightButtonClickedCallback,
	DCS_CenterButtonClickedCallback,
	DCS_BLeftButtonClickedCallback,
	DCS_BRightButtonClickedCallback,
	DCS_LeftButtonPressedCallback,
	DCS_RightButtonPressedCallback,
	DCS_CenterButtonPressedCallback,
	DCS_BLeftButtonPressedCallback,
	DCS_BRightButtonPressedCallback
};
