#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"
#include "linkMonitor.h"

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

uint8_t indexOfGeneralSettingMenu;
static uint8_t currentIndex;
/* UI order only: retain the existing editor IDs and Flash field meanings. */
static const uint8_t menuOrder[] = {0, 6, 2, 7, 1, 3, 4, 5, 8};
#define GENERAL_MENU_COUNT (sizeof(menuOrder)/sizeof(menuOrder[0]))
void Screen_GeneralSettingsResetSelection(void) { currentIndex = 0; }
static void window_1_callback(UG_MESSAGE *msg)
{
}
void GSS_LeftButtonClickedCallback(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (currentIndex) currentIndex--;
}
void GSS_LeftButtonPressedCallback(void)
{
}
void GSS_RightButtonClickedCallback(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (currentIndex + 1 < GENERAL_MENU_COUNT) currentIndex++;
}
void GSS_RightButtonPressedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	
	UG_WindowHide(&window_1);
  	UG_WindowDelete(&window_1);
	currentScreen = &mainMenuScreen;
	currentScreen->init();
	currentScreen->render();
}
void GSS_CenterButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	indexOfGeneralSettingMenu = menuOrder[currentIndex];
	UG_WindowHide(&window_1);
  UG_WindowDelete(&window_1);
	currentScreen = indexOfGeneralSettingMenu == 8 ? &dateTimeScreen : &editGeneralSettingScreen;
	currentScreen->init();
	currentScreen->render();
}
void GSS_CenterButtonPressedCallback(void)
{
}
void GSS_BLeftButtonClickedCallback(void)
{
}
void GSS_BLeftButtonPressedCallback(void)
{
}
void GSS_BRightButtonClickedCallback(void)
{
}
void GSS_BRightButtonPressedCallback(void)
{
}
void GSS_Init(void)
{
    /* Editor returns keep the selected item/page; a fresh menu entry resets it. */
    Screen_SettingsMenuCreateStacked("General setup");
}
void GSS_Render(void)
{
    char *labels[] = {"Enable sleep mode", "Link scan interval", "Sleep check interval", "LoRa network ID",
                      "LoRa frequency", "LoRa spread factor", "Sleep after idle", "Link timeout", "Date & time"};
    uint8_t page = currentIndex / 4 * 4;
    Screen_SettingsMenuBegin(page);
    Screen_SettingsMenuPage(page, GENERAL_MENU_COUNT);
    for (uint8_t row = 0; row < 4; row++) {
        uint8_t item;
        char value[32] = {0};
        if (page + row >= GENERAL_MENU_COUNT) { Screen_SettingsMenuRow(row, NULL, NULL, 0); continue; }
        item = menuOrder[page + row];
        switch (item) {
            case 0: strcpy(value, systemInfo.sleepMode ? "Yes" : "No"); break;
            case 1: sprintf(value, "%lu s", (unsigned long)systemInfo.tUpdate); break;
            case 2: sprintf(value, "%lu s", (unsigned long)systemInfo.tSleepUpdate); break;
            case 3: sprintf(value, "%02X", systemInfo.netIdConfig); break;
            case 4: sprintf(value, "%d MHz", systemInfo.frequencyConfig); break;
            case 5: sprintf(value, "SF%u", systemInfo.sfConfig); break;
            case 6: sprintf(value, "%u s", Link_GetIdleSeconds()); break;
            case 7: sprintf(value, "%u s", Link_GetTimeoutSeconds()); break;
            case 8: sprintf(value, "%02u/%02u/%04u %02u:%02u", systemInfo.date.Date,
                systemInfo.date.Month, 2000u+systemInfo.date.Year, systemInfo.time.Hours, systemInfo.time.Minutes); break;
        }
        Screen_SettingsMenuRow(row, labels[item], value, page + row == currentIndex);
    }
    Screen_SettingsMenuUpdate();
}
Screen_t generalSettingScreen = 
{ 
	0,
	GSS_Init,
	GSS_Render,
	GSS_LeftButtonClickedCallback,
	GSS_RightButtonClickedCallback,
	GSS_CenterButtonClickedCallback,
	GSS_BLeftButtonClickedCallback,
	GSS_BRightButtonClickedCallback,
	GSS_LeftButtonPressedCallback,
	GSS_RightButtonPressedCallback,
	GSS_CenterButtonPressedCallback,
	GSS_BLeftButtonPressedCallback,
	GSS_BRightButtonPressedCallback
};
