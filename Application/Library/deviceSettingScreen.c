#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"
#include "linkMonitor.h"

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

uint8_t indexOfSettingMenu;
static uint8_t menuIndex;
void Screen_DeviceSettingsResetSelection(void) { menuIndex = 0; }
static void window_1_callback(UG_MESSAGE *msg)
{
}
void DSS_LeftButtonClickedCallback(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (menuIndex) menuIndex--;
}
void DSS_LeftButtonPressedCallback(void)
{
}
void DSS_RightButtonClickedCallback(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (menuIndex + 1 < 7) menuIndex++;
}
void DSS_RightButtonPressedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	
	UG_WindowHide(&window_1);
 	UG_WindowDelete(&window_1);
	currentScreen = &deviceControlScreen;
	currentScreen->init();
	currentScreen->render();
}
void DSS_CenterButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	indexOfSettingMenu = menuIndex;
	UG_WindowHide(&window_1);
  	UG_WindowDelete(&window_1);
	currentScreen = &editDeviceSettingScreen;
	currentScreen->init();
	currentScreen->render();
}
void DSS_CenterButtonPressedCallback(void)
{
}
void DSS_BLeftButtonClickedCallback(void)
{
}
void DSS_BLeftButtonPressedCallback(void)
{
}
void DSS_BRightButtonClickedCallback(void)
{
}
void DSS_BRightButtonPressedCallback(void)
{
}
void DSS_Init(void)
{
    /* Keep the row/page on editor return; reset only on fresh control entry. */
    Screen_SettingsMenuCreateStacked("Device setup");
}
void DSS_Render(void)
{
    char *labels[] = {"Low-current alarm", "Alarm delay", "Lock", "Hide", "Delete device", "Device name", "Auto status interval"};
    MotorData_t *module = &deviceBuff[systemInfo.currentDeviceId].motorData[systemInfo.currentModuleId];
    uint8_t page = menuIndex / 4 * 4;
    Screen_SettingsMenuBegin(page);
    Screen_SettingsMenuPage(page, 7);
    for (uint8_t row = 0; row < 4; row++) {
        uint8_t item = page + row;
        char value[32] = {0};
        if (item >= 7) { Screen_SettingsMenuRow(row, NULL, NULL, 0); continue; }
        switch (item) {
            case 0: sprintf(value, "%.2f A", module->alarmThreshold); break;
            case 1: sprintf(value, "%lu s", (unsigned long)module->alarmDelayTime); break;
            case 2: strcpy(value, module->isLock ? "Yes" : "No"); break;
            case 3: strcpy(value, module->isHide ? "Yes" : "No"); break;
            case 4: strcpy(value, deviceBuff[systemInfo.currentDeviceId].isDelete ? "Yes" : "No"); break;
            case 5: snprintf(value, sizeof(value), "%.19s", module->name); break;
            case 6: sprintf(value, "%u s", Link_GetAutoStatus(systemInfo.currentDeviceId)); break;
        }
        Screen_SettingsMenuRow(row, labels[item], value, item == menuIndex);
    }
    Screen_SettingsMenuUpdate();
}
Screen_t deviceSettingScreen = 
{ 
	0,
	DSS_Init,
	DSS_Render,
	DSS_LeftButtonClickedCallback,
	DSS_RightButtonClickedCallback,
	DSS_CenterButtonClickedCallback,
	DSS_BLeftButtonClickedCallback,
	DSS_BRightButtonClickedCallback,
	DSS_LeftButtonPressedCallback,
	DSS_RightButtonPressedCallback,
	DSS_CenterButtonPressedCallback,
	DSS_BLeftButtonPressedCallback,
	DSS_BRightButtonPressedCallback
};
