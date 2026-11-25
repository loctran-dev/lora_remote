#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"
#include "link_monitor.h"

#define GENERAL_MENU_COUNT (sizeof(menuOrder) / sizeof(menuOrder[0]))
#define ROWS_PER_PAGE      4
#define DATE_TIME_ITEM     8

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

uint8_t indexOfGeneralSettingMenu;

static uint8_t currentIndex;
/* UI order only: retain the existing editor IDs and Flash field meanings. */
static const uint8_t menuOrder[] = {0, 6, 2, 7, 1, 3, 4, 5, DATE_TIME_ITEM};

static void init(void);
static void render(void);
static void leftClicked(void);
static void rightClicked(void);
static void centerClicked(void);
static void rightPressed(void);
static void noAction(void);

Screen_t generalSettingScreen = {
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

/** @brief Select the first item next time the general setup menu opens. */
void ScrnGss_ResetSelection(void)
{
    currentIndex = 0;
}

static void init(void)
{
    /* Editor returns keep the selected item/page; a fresh menu entry resets it. */
    ScrnCommon_SettingsMenuCreateStacked("General setup");
}

static void render(void)
{
    char *labels[] = {"Enable sleep mode", "Link scan interval", "Sleep check interval", "LoRa network ID",
                      "LoRa frequency", "LoRa spread factor", "Sleep after idle", "Link timeout", "Date & time"};
    uint8_t page = currentIndex / ROWS_PER_PAGE * ROWS_PER_PAGE;
    uint8_t item;
    char value[32];

    ScrnCommon_SettingsMenuBegin(page);
    ScrnCommon_SettingsMenuPage(page, GENERAL_MENU_COUNT);
    for (uint8_t row = 0; row < ROWS_PER_PAGE; row++)
    {
        value[0] = '\0';
        if (page + row >= GENERAL_MENU_COUNT)
        {
            ScrnCommon_SettingsMenuRow(row, NULL, NULL, 0);
            continue;
        }
        item = menuOrder[page + row];
        switch (item)
        {
            case 0:
                strcpy(value, systemInfo.sleepMode ? "Yes" : "No");
                break;
            case 1:
                sprintf(value, "%lu s", (unsigned long)systemInfo.tUpdate);
                break;
            case 2:
                sprintf(value, "%lu s", (unsigned long)systemInfo.tSleepUpdate);
                break;
            case 3:
                sprintf(value, "%02X", systemInfo.netIdConfig);
                break;
            case 4:
                sprintf(value, "%d MHz", systemInfo.frequencyConfig);
                break;
            case 5:
                sprintf(value, "SF%u", systemInfo.sfConfig);
                break;
            case 6:
                sprintf(value, "%u s", Link_GetIdleSeconds());
                break;
            case 7:
                sprintf(value, "%u s", Link_GetTimeoutSeconds());
                break;
            case DATE_TIME_ITEM:
                sprintf(value, "%02u/%02u/%04u %02u:%02u", systemInfo.date.Date, systemInfo.date.Month,
                    2000u + systemInfo.date.Year, systemInfo.time.Hours, systemInfo.time.Minutes);
                break;
            default:
                break;
        }
        ScrnCommon_SettingsMenuRow(row, labels[item], value, page + row == currentIndex);
    }
    ScrnCommon_SettingsMenuUpdate();
}

static void leftClicked(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    if (currentIndex)
    {
        currentIndex--;
    }
}

static void rightClicked(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    if (currentIndex + 1 < GENERAL_MENU_COUNT)
    {
        currentIndex++;
    }
}

static void centerClicked(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    indexOfGeneralSettingMenu = menuOrder[currentIndex];
    ScrnCommon_Switch(indexOfGeneralSettingMenu == DATE_TIME_ITEM ? &dateTimeScreen : &editGeneralSettingScreen);
}

/* Long press on RIGHT returns to the main menu. */
static void rightPressed(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    ScrnCommon_Switch(&mainMenuScreen);
}

static void noAction(void)
{
}
