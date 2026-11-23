#include "lcd.h"
#include "screens.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "image_data.h"
#include "main.h"

#define EDIT_CHARACTERS_MAX 19

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];
extern SystemStorageData_t storageData;

/* uGUI objects shared by every screen. */
UG_WINDOW window_1;
UG_OBJECT obj_buff_wnd_1[MAX_OBJECTS];
UG_TEXTBOX textbox_1;
UG_TEXTBOX textbox_2;
UG_TEXTBOX textbox_3;
UG_TEXTBOX textbox_4;
UG_TEXTBOX textbox_5;
UG_TEXTBOX textbox_6;
UG_TEXTBOX textbox_7;
UG_TEXTBOX textbox_8;
UG_TEXTBOX textbox_9;
UG_TEXTBOX textbox_10;
UG_TEXTBOX textbox_11;
UG_TEXTBOX textbox_12;
UG_TEXTBOX textbox_13;
UG_TEXTBOX textbox_14;
UG_TEXTBOX textbox_15;
UG_TEXTBOX textbox_16;
UG_TEXTBOX textbox_17;
UG_TEXTBOX textbox_18;
UG_TEXTBOX textbox_19;
UG_TEXTBOX textbox_20;
UG_IMAGE image_1;
UG_IMAGE image_2;
UG_IMAGE image_3;
UG_IMAGE image_4;
UG_GIF gif_1;
UG_GIF gif_2;
UG_REALEDIT real_edit_1;

/* Settings menu state. */
static uint8_t settingsPage;
static uint8_t settingsPageChanged;
static uint8_t settingsRows;
static uint8_t settingsStacked;

/* Character editor state. */
static UG_TEXTBOX editCharacters[EDIT_CHARACTERS_MAX];
static uint8_t editCharacterCount;

static void windowCallback(UG_MESSAGE *message);
static void createWindow(void);
static void createText(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye,
    char *text, UG_COLOR background);
static void createSettingsMenu(char *title, uint8_t stacked);
static void saveRadioSettings(void);
/* Device visibility and selection ========================================== */

/* Hide is device-level in the current single-module storage layout. */
uint8_t Screen_DeviceVisible(uint8_t id)
{
    return id < MAXIMUM_DEVICE && !deviceBuff[id].isDelete &&
        !deviceBuff[id].motorData[0].isHide;
}

uint8_t Screen_EnsureVisibleDevice(void)
{
    uint8_t start = systemInfo.currentDeviceId < MAXIMUM_DEVICE ? systemInfo.currentDeviceId : 0;
    uint8_t id;

    /* Prefer the next device, then the closest preceding visible device. */
    for (int offset = 0; offset < MAXIMUM_DEVICE; offset++)
    {
        id = start + offset < MAXIMUM_DEVICE ? start + offset : MAXIMUM_DEVICE - offset - 1;
        if (Screen_DeviceVisible(id))
        {
            if (id != systemInfo.currentDeviceId || systemInfo.currentModuleId >= 6)
            {
                systemInfo.currentModuleId = 0;
            }
            systemInfo.currentDeviceId = id;
            return 1;
        }
    }

    /* Keep indices in range even when the control screen has no selection. */
    systemInfo.currentDeviceId = 0;
    systemInfo.currentModuleId = 0;
    return 0;
}

void Screen_SelectAdjacentVisible(int8_t direction)
{
    int id = systemInfo.currentDeviceId;

    for (id += direction; id >= 0 && id < MAXIMUM_DEVICE; id += direction)
    {
        if (Screen_DeviceVisible((uint8_t)id))
        {
            systemInfo.currentDeviceId = id;
            systemInfo.currentModuleId = 0;
            return;
        }
    }
}

/* Settings persistence ===================================================== */

void Screen_SaveDeviceFlags(uint8_t id)
{
    DeviceStorageData_t *stored;
    MotorData_t *module;

    if (id >= MAXIMUM_DEVICE)
    {
        return;
    }
    stored = &storageData.DeviceConfig[id];
    module = &deviceBuff[id].motorData[0];
    if (stored->isHide != module->isHide || stored->isLock != module->isLock)
    {
        stored->isHide = module->isHide;
        stored->isLock = module->isLock;
        systemInfo.isNeedSaveConfig = 1;
    }
}

void Screen_SaveDeviceSettings(uint8_t id)
{
    DeviceStorageData_t *stored;
    MotorData_t *module;

    if (id >= MAXIMUM_DEVICE)
    {
        return;
    }
    stored = &storageData.DeviceConfig[id];
    module = &deviceBuff[id].motorData[0];
    stored->alarmThreshold = module->alarmThreshold;
    stored->alarmDelayTime = module->alarmDelayTime;
    stored->isLock = module->isLock;
    stored->isHide = module->isHide;
    stored->isDelete = deviceBuff[id].isDelete;
    memcpy(stored->name, module->name, sizeof(stored->name));
    stored->name[sizeof(stored->name) - 1] = 0;
    systemInfo.isNeedSaveConfig = 1;
}

void Screen_SaveGeneralSettings(void)
{
    saveRadioSettings();
    storageData.sleepMode = systemInfo.sleepMode;
    storageData.tUpdate = systemInfo.tUpdate;
    storageData.tSleepUpdate = systemInfo.tSleepUpdate;
    Main_UpdateStatusPeriod();
}

/* Status bar =============================================================== */

/* Every screen reserves textbox 0/1 and image 0 for this same status bar.
   Bitmap bounds (15x9) do not overlap the four-character 100% textbox. */
void Screen_StatusBarCreate(void)
{
    createText(&textbox_1, 0, 0, 0, 50, 15, "", C_BLACK);
    createText(&textbox_2, 1, 104, 0, 127, 15, "", C_BLACK);
    UG_TextboxSetAlignment(&window_1, 1, ALIGN_CENTER_RIGHT);
    UG_TextboxSetScrollEnabled(&window_1, 0, 0);
    UG_TextboxSetScrollEnabled(&window_1, 1, 0);
    UG_ImageCreate(&window_1, &image_1, IMG_ID_0, 87, 3, 101, 11);
    Screen_StatusBarUpdate();
}

void Screen_StatusBarUpdate(void)
{
    char time[8];

    sprintf(time, "%02u:%02u", systemInfo.time.Hours, systemInfo.time.Minutes);
    UG_TextboxSetText(&window_1, 0, time);
    Screen_UpdateBattery();
}

void Screen_UpdateBattery(void)
{
    char text[8];
    uint8_t valid = systemInfo.batteryVoltage > 0;
    uint8_t frame;

    /* Percentage text. */
    if (valid)
    {
        sprintf(text, "%u%%", systemInfo.batteryUsage);
    }
    else
    {
        strcpy(text, "--%");
    }
    UG_TextboxSetText(&window_1, TXB_ID_1, text);
    UG_TextboxSetScrollEnabled(&window_1, TXB_ID_1, 0);
    UG_TextboxSetForeColor(&window_1, TXB_ID_1,
        valid && systemInfo.batteryUsage <= 10 ? C_RED : C_WHITE);

    /* Battery icon; animated while charging. */
    frame = !valid ? 0 : systemInfo.batteryUsage <= 5 ? 0 :
        systemInfo.batteryUsage <= 25 ? 1 : systemInfo.batteryUsage <= 50 ? 2 :
        systemInfo.batteryUsage <= 75 ? 3 : 4;
    if (systemInfo.isChanging && valid)
    {
        frame = (HAL_GetTick() / 500) % 5;
    }
    UG_ImageSetBMP(&window_1, IMG_ID_0, batteryGIF[frame]);
}

/* Settings menu ============================================================ */

void Screen_SettingsMenuCreate(char *title)
{
    createSettingsMenu(title, 0);
}

void Screen_SettingsMenuCreateStacked(char *title)
{
    createSettingsMenu(title, 1);
}

void Screen_SettingsMenuPage(uint8_t page, uint8_t count)
{
    char text[12];

    if (settingsStacked)
    {
        sprintf(text, "%u/%u", page / settingsRows + 1, (count + settingsRows - 1) / settingsRows);
        UG_TextboxSetText(&window_1, 21, text);
    }
}

void Screen_SettingsMenuBegin(uint8_t page)
{
    settingsPageChanged = page != settingsPage;
    if (settingsPageChanged)
    {
        UG_FillFrame(0, 42, 127, settingsStacked ? 139 : 132, C_BLACK);
        settingsPage = page;
    }
}

void Screen_SettingsMenuRow(uint8_t row, char *label, char *value, uint8_t selected)
{
    uint8_t id;

    if (row >= settingsRows)
    {
        return;
    }
    for (uint8_t column = 0; column < 2; column++)
    {
        id = 6 + row * 2 + column;
        if (!label)
        {
            if (UG_TextboxCheckVisible(&window_1, id))
            {
                UG_TextboxHide(&window_1, id);
            }
            continue;
        }
        if (settingsPageChanged || !UG_TextboxCheckVisible(&window_1, id))
        {
            UG_TextboxShow(&window_1, id);
        }
        UG_TextboxSetText(&window_1, id, column ? value : label);
        UG_TextboxSetBackColor(&window_1, id, selected ? C_DIM_GRAY : C_BLACK);
        UG_TextboxSetScrollEnabled(&window_1, id, settingsStacked && column ? 0 : selected);
    }
}

void Screen_SettingsMenuUpdate(void)
{
    Screen_StatusBarUpdate();
    UG_Update();
}

/* Empty device control screen ============================================== */

void Screen_EmptyControlInit(void)
{
    createWindow();
    Screen_StatusBarCreate();
    createText(&textbox_6, 5, 0, 20, 127, 37, "Device control", C_DARK_SLATE_BLUE);
    createText(&textbox_7, 6, 0, 48, 127, 65, "No visible devices", C_BLACK);
    createText(&textbox_8, 8, 0, 76, 127, 93, "Use Device manager", C_BLACK);
    createText(&textbox_9, 10, 0, 94, 127, 111, "to show devices", C_BLACK);
    createText(&textbox_4, 3, 0, 144, 127, 159, "Device manager", C_RED);
    UG_WindowShow(&window_1);
    UG_Update();
}

void Screen_EmptyControlRender(void)
{
    Screen_StatusBarUpdate();
    UG_Update();
}

/* Menu scrolling and character editor ====================================== */

void Screen_MenuScrollUpdate(uint8_t count, uint8_t selected)
{
    for (uint8_t i = 0; i < count; i++)
    {
        UG_TextboxSetScrollEnabled(&window_1, TXB_ID_6 + i * 2, i == selected);
        UG_TextboxSetScrollEnabled(&window_1, TXB_ID_7 + i * 2, i == selected);
    }
}

void Screen_EditCharactersCreate(uint8_t count)
{
    uint8_t columns;
    uint16_t x;
    uint16_t y;

    editCharacterCount = count > EDIT_CHARACTERS_MAX ? EDIT_CHARACTERS_MAX : count;
    UG_TextboxHide(&window_1, TXB_ID_7);
    for (uint8_t i = 0; i < editCharacterCount; i++)
    {
        columns = count > 10 ? 10 : count;
        x = count > 10 ? 4 + (i % 10) * 12 : (128 - columns * 12) / 2 + i * 12;
        y = 57 + (i / columns) * 20;
        UG_TextboxCreate(&window_1, &editCharacters[i], 32 + i, x, y, x + 10, y + 16);
        UG_TextboxSetFont(&window_1, 32 + i, DEFAULT_FONT);
        UG_TextboxSetAlignment(&window_1, 32 + i, ALIGN_CENTER);
    }
}

void Screen_EditCharactersUpdate(uint8_t position)
{
    char *text = UG_TextboxGetText(&window_1, TXB_ID_7);
    uint8_t length;
    char character[2];

    if (text == NULL)
    {
        return;
    }
    length = strlen(text);
    for (uint8_t i = 0; i < editCharacterCount; i++)
    {
        character[0] = i < length ? text[i] : ' ';
        character[1] = '\0';
        UG_TextboxSetText(&window_1, 32 + i, character);
        UG_TextboxSetBackColor(&window_1, 32 + i, i == position ? C_YELLOW : C_BLACK);
        UG_TextboxSetForeColor(&window_1, 32 + i, i == position ? C_BLACK : C_WHITE);
    }
    free(text);
}

/* Private helpers ========================================================== */

static void windowCallback(UG_MESSAGE *message)
{
    (void)message;
}

static void createWindow(void)
{
    UG_WindowCreate(&window_1, obj_buff_wnd_1, MAX_OBJECTS, windowCallback);
    UG_WindowSetStyle(&window_1, WND_STYLE_HIDE_TITLE);
    UG_WindowSetTitleHeight(&window_1, 0);
    UG_WindowSetBackColor(&window_1, C_BLACK);
    UG_WindowSetXStart(&window_1, 0);
    UG_WindowSetYStart(&window_1, 0);
    UG_WindowSetXEnd(&window_1, LCD_WIDTH - 1);
    UG_WindowSetYEnd(&window_1, LCD_HEIGHT - 1);
}

static void createText(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye,
    char *text, UG_COLOR background)
{
    UG_TextboxCreate(&window_1, box, id, x, y, xe, ye);
    UG_TextboxSetFont(&window_1, id, DEFAULT_FONT);
    UG_TextboxSetText(&window_1, id, text);
    UG_TextboxSetBackColor(&window_1, id, background);
    UG_TextboxSetForeColor(&window_1, id, C_WHITE);
    UG_TextboxSetAlignment(&window_1, id, ALIGN_CENTER_LEFT);
}

static void createSettingsMenu(char *title, uint8_t stacked)
{
    UG_TEXTBOX *labels[] = {&textbox_7, &textbox_9, &textbox_11, &textbox_13, &textbox_15};
    UG_TEXTBOX *values[] = {&textbox_8, &textbox_10, &textbox_12, &textbox_14, &textbox_16};
    int y;

    settingsPage = 255;
    settingsStacked = stacked;
    settingsRows = stacked ? 4 : 5;

    /* Window, status bar and title (with page indicator when stacked). */
    createWindow();
    Screen_StatusBarCreate();
    createText(&textbox_6, 20, 0, 20, stacked ? 99 : 127, 37, title, C_DARK_SLATE_BLUE);
    if (stacked)
    {
        createText(&textbox_17, 21, 100, 20, 127, 37, "", C_DARK_SLATE_BLUE);
        UG_TextboxSetAlignment(&window_1, 21, ALIGN_CENTER);
        UG_TextboxSetScrollEnabled(&window_1, 21, 0);
    }

    /* Rows: label above value when stacked, side by side otherwise. */
    for (uint8_t row = 0; row < settingsRows; row++)
    {
        if (stacked)
        {
            y = 42 + row * 24;
            createText(labels[row], 6 + row * 2, 3, y, 124, y + 10, "", C_BLACK);
            createText(values[row], 7 + row * 2, 3, y + 11, 124, y + 21, "", C_BLACK);
            UG_TextboxSetAlignment(&window_1, 7 + row * 2, ALIGN_CENTER_RIGHT);
            UG_TextboxSetForeColor(&window_1, 7 + row * 2, C_CYAN);
            UG_TextboxSetScrollEnabled(&window_1, 7 + row * 2, 0);
        }
        else
        {
            createText(labels[row], 6 + row * 2, 0, 42 + row * 18, 80, 58 + row * 18, "", C_BLACK);
            createText(values[row], 7 + row * 2, 81, 42 + row * 18, 127, 58 + row * 18, "", C_BLACK);
        }
    }

    /* Button help. */
    createText(&textbox_3, 2, 0, 144, 35, 159, "Up", C_RED);
    createText(&textbox_4, 3, 36, 144, 91, 159, "Change", C_RED);
    createText(&textbox_5, 4, 92, 144, 127, 159, "Down", C_RED);
    UG_WindowShow(&window_1);
}

static void saveRadioSettings(void)
{
    storageData.netIdConfig = systemInfo.netIdConfig;
    storageData.frequencyConfig = systemInfo.frequencyConfig;
    storageData.sfConfig = systemInfo.sfConfig;
    systemInfo.isNeedSaveConfig = 1;
}
