#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"
#include "link_monitor.h"

#define ITEM_COUNT    7
#define ROWS_PER_PAGE 4

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

uint8_t indexOfSettingMenu;

static uint8_t menuIndex;

static void init(void);
static void render(void);
static void leftClicked(void);
static void rightClicked(void);
static void centerClicked(void);
static void rightPressed(void);
static void noAction(void);

Screen_t deviceSettingScreen = {
    0,
    init,
    render,
    leftClicked,
    rightClicked,
    centerClicked,
    noAction,
    noAction,
    noAction,
    rightPressed,
    noAction,
    noAction,
    noAction
};

/* Public API =============================================================== */

void ScrnDss_ResetSelection(void)
{
    menuIndex = 0;
}

/* Screen handlers ========================================================== */

static void init(void)
{
    /* Keep the row/page on editor return; reset only on fresh control entry. */
    ScrnCommon_SettingsMenuCreateStacked("Device setup");
}

static void render(void)
{
    char *labels[] = {"Low-current alarm", "Alarm delay", "Lock", "Hide", "Delete device", "Device name",
                      "Auto status interval"};
    MotorData_t *module = &deviceBuff[systemInfo.currentDeviceId].motorData[systemInfo.currentModuleId];
    uint8_t page = menuIndex / ROWS_PER_PAGE * ROWS_PER_PAGE;
    uint8_t item;
    char value[32];

    ScrnCommon_SettingsMenuBegin(page);
    ScrnCommon_SettingsMenuPage(page, ITEM_COUNT);
    for (uint8_t row = 0; row < ROWS_PER_PAGE; row++)
    {
        item = page + row;
        value[0] = '\0';
        if (item >= ITEM_COUNT)
        {
            ScrnCommon_SettingsMenuRow(row, NULL, NULL, 0);
            continue;
        }
        switch (item)
        {
            case 0:
                sprintf(value, "%.2f A", module->alarmThreshold);
                break;
            case 1:
                sprintf(value, "%lu s", (unsigned long)module->alarmDelayTime);
                break;
            case 2:
                strcpy(value, module->isLock ? "Yes" : "No");
                break;
            case 3:
                strcpy(value, module->isHide ? "Yes" : "No");
                break;
            case 4:
                strcpy(value, deviceBuff[systemInfo.currentDeviceId].isDelete ? "Yes" : "No");
                break;
            case 5:
                snprintf(value, sizeof(value), "%.19s", module->name);
                break;
            case 6:
                sprintf(value, "%u s", Link_GetAutoStatus(systemInfo.currentDeviceId));
                break;
            default:
                break;
        }
        ScrnCommon_SettingsMenuRow(row, labels[item], value, item == menuIndex);
    }
    ScrnCommon_SettingsMenuUpdate();
}

static void leftClicked(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (menuIndex)
    {
        menuIndex--;
    }
}

static void rightClicked(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (menuIndex + 1 < ITEM_COUNT)
    {
        menuIndex++;
    }
}

static void centerClicked(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    indexOfSettingMenu = menuIndex;
    ScrnCommon_Switch(&editDeviceSettingScreen);
}

/* Long press on RIGHT returns to device control. */
static void rightPressed(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    ScrnCommon_Switch(&deviceControlScreen);
}

static void noAction(void)
{
}
