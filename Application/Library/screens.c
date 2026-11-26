#include "lcd.h"
#include "screens.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "image_data.h"
#include "main.h"
#include "config_store.h"

#define EDIT_CHARACTERS_MAX 19

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

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
static int selectionPosition(void);
static uint8_t selectPosition(int position);
static int nearestVisible(int position, int8_t direction);

/**
 * @brief  Close the current window and open another screen (init() then render()).
 * @param  next  Screen to open.
 * @note   Call with uiMutex held. To start the screen with fresh state, call
 *         its reset API (for example ScrnDss_ResetSelection()) just before.
 */
void ScrnCommon_Switch(Screen_t *next)
{
    UG_WindowHide(&window_1);
    UG_WindowDelete(&window_1);
    currentScreen = next;
    currentScreen->init();
    currentScreen->render();
}

/**
 * @brief  Whether a device is paired and has at least one visible motor.
 * @param  id  Device index in deviceBuff.
 * @return 1 if visible (device lists: history picker, connection notices).
 */
uint8_t ScrnCommon_DeviceVisible(uint8_t id)
{
    return id < MAXIMUM_DEVICE && !storageData.DeviceConfig[id].deleted && !Config_DeviceHidden(id);
}

/**
 * @brief  Whether a motor is paired, has a settings slot and is not hidden.
 * @param  device  Device index in deviceBuff.
 * @param  module  Module index on that device.
 */
uint8_t ScrnCommon_MotorVisible(uint8_t device, uint8_t module)
{
    const MotorStorageData_t *motor;

    if (device >= MAXIMUM_DEVICE || storageData.DeviceConfig[device].deleted)
    {
        return 0;
    }
    motor = Config_Motor(device, module);
    return motor != NULL && !motor->hidden;
}

/** @brief Whether the selected motor (currentDeviceId/currentModuleId) is visible. */
uint8_t ScrnCommon_SelectionVisible(void)
{
    return ScrnCommon_MotorVisible(systemInfo.currentDeviceId, systemInfo.currentModuleId);
}

/**
 * @brief  Make the selection point to a visible motor.
 *
 * Motors are ordered flat by device, then module (M11, M12, ..., M21, ...).
 * Keeps the selected motor if visible, else picks the next visible one, else
 * the closest preceding one.
 *
 * @return 1 if a visible motor is selected, 0 if there is none (indices set to 0).
 */
uint8_t ScrnCommon_EnsureVisibleDevice(void)
{
    int position;

    if (ScrnCommon_SelectionVisible())
    {
        return 1;
    }
    position = selectionPosition();
    if (selectPosition(nearestVisible(position, 1)) || selectPosition(nearestVisible(position, -1)))
    {
        return 1;
    }

    /* Keep indices in range even when the control screen has no selection. */
    systemInfo.currentDeviceId = 0;
    systemInfo.currentModuleId = 0;
    return 0;
}

/**
 * @brief  Select the next or previous visible motor in the flat device/module
 *         order; stays put at the end of the list.
 * @param  direction  +1 for the next motor, -1 for the previous one.
 */
void ScrnCommon_SelectAdjacentVisible(int8_t direction)
{
    int current;
    int target;

    current = selectionPosition();
    target = nearestVisible(current, direction);
    selectPosition(target);
}

/**
 * @brief  Settings of the selected motor (systemInfo.currentDeviceId/currentModuleId).
 * @return Its MotorConfig slot, or a blank placeholder if it has none, so screens
 *         never dereference NULL; writes to the placeholder are discarded.
 */
MotorStorageData_t *ScrnCommon_CurrentMotor(void)
{
    static MotorStorageData_t noMotor;
    MotorStorageData_t *motor = Config_Motor(systemInfo.currentDeviceId, systemInfo.currentModuleId);

    if (motor)
    {
        return motor;
    }
    memset(&noMotor, 0, sizeof(noMotor));
    noMotor.device = MOTOR_SLOT_FREE;
    strcpy(noMotor.name, "Default");
    return &noMotor;
}

/**
 * @brief  Runtime state of the selected motor (systemInfo.currentDeviceId/currentModuleId).
 * @return Its motorBuff entry, or a blank placeholder if the motor has no slot,
 *         so screens never dereference NULL; writes to the placeholder are discarded.
 */
MotorData_t *ScrnCommon_CurrentMotorState(void)
{
    static MotorData_t noMotor;
    MotorData_t *motor = Main_MotorState(systemInfo.currentDeviceId, systemInfo.currentModuleId);

    if (motor)
    {
        return motor;
    }
    memset(&noMotor, 0, sizeof(noMotor));
    return &noMotor;
}

/**
 * @brief  Display name of a motor.
 * @return Its stored name, or "Default" if it has no MotorConfig slot.
 */
const char *ScrnCommon_MotorName(uint8_t device, uint8_t module)
{
    const MotorStorageData_t *motor = Config_Motor(device, module);

    return motor ? motor->name : "Default";
}

/**
 * @brief  Commit device or motor settings edited in storageData.
 * @note   Flash is written later by systemTask (configSavePending).
 */
void ScrnCommon_SaveDeviceSettings(void)
{
    systemInfo.configSavePending = 1;
}

/**
 * @brief  Commit the general settings edited in storageData: save them and apply the new periods.
 * @note   Flash is written later by systemTask (configSavePending).
 */
void ScrnCommon_SaveGeneralSettings(void)
{
    systemInfo.configSavePending = 1;
    Main_UpdateStatusPeriod();
}

/**
 * @brief  Create the status bar: clock (textbox 0), battery % (textbox 1) and battery icon (image 0).
 * @note   Every screen reserves these widget ids for the status bar. The
 *         15x9 icon does not overlap the four-character "100%" textbox.
 */
void ScrnCommon_StatusBarCreate(void)
{
    createText(&textbox_1, 0, 0, 0, 50, 15, "", C_BLACK);
    createText(&textbox_2, 1, 104, 0, 127, 15, "", C_BLACK);
    UG_TextboxSetAlignment(&window_1, 1, ALIGN_CENTER_RIGHT);
    UG_TextboxSetScrollEnabled(&window_1, 0, 0);
    UG_TextboxSetScrollEnabled(&window_1, 1, 0);
    UG_ImageCreate(&window_1, &image_1, IMG_ID_0, 87, 3, 101, 11);
    ScrnCommon_StatusBarUpdate();
}

/** @brief Refresh the clock and the battery indicator. */
void ScrnCommon_StatusBarUpdate(void)
{
    char time[8];

    sprintf(time, "%02u:%02u", systemInfo.time.Hours, systemInfo.time.Minutes);
    UG_TextboxSetText(&window_1, 0, time);
    ScrnCommon_UpdateBattery();
}

/** @brief Refresh the battery percentage and icon; red at 10 % or less, animated while charging. */
void ScrnCommon_UpdateBattery(void)
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
    if (systemInfo.charging && valid)
    {
        frame = (HAL_GetTick() / 500) % 5;
    }
    UG_ImageSetBMP(&window_1, IMG_ID_0, batteryGIF[frame]);
}

/**
 * @brief  Create a settings menu with label and value on one row.
 * @param  title  Menu title.
 */
void ScrnCommon_SettingsMenuCreate(char *title)
{
    createSettingsMenu(title, 0);
}

/**
 * @brief  Create a settings menu with the value below the label, and a page counter.
 * @param  title  Menu title.
 */
void ScrnCommon_SettingsMenuCreateStacked(char *title)
{
    createSettingsMenu(title, 1);
}

/**
 * @brief  Show "page/pages" on a stacked menu; does nothing on a one-line menu.
 * @param  page   Index of the first row on the page.
 * @param  count  Number of menu items.
 */
void ScrnCommon_SettingsMenuPage(uint8_t page, uint8_t count)
{
    char text[12];

    if (settingsStacked)
    {
        sprintf(text, "%u/%u", page / settingsRows + 1, (count + settingsRows - 1) / settingsRows);
        UG_TextboxSetText(&window_1, 21, text);
    }
}

/**
 * @brief  Start drawing a menu page; clears the rows when the page changed.
 * @param  page  Index of the first row on the page.
 */
void ScrnCommon_SettingsMenuBegin(uint8_t page)
{
    settingsPageChanged = page != settingsPage;
    if (settingsPageChanged)
    {
        UG_FillFrame(0, 42, 127, settingsStacked ? 139 : 132, C_BLACK);
        settingsPage = page;
    }
}

/**
 * @brief  Draw one menu row.
 * @param  row       Row on the page.
 * @param  label     Item label, or NULL to hide the row.
 * @param  value     Item value.
 * @param  selected  1 to highlight the row.
 */
void ScrnCommon_SettingsMenuRow(uint8_t row, char *label, char *value, uint8_t selected)
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

/** @brief Finish a menu page: refresh the status bar and redraw. */
void ScrnCommon_SettingsMenuUpdate(void)
{
    ScrnCommon_StatusBarUpdate();
    UG_Update();
}

/** @brief Build the device control screen shown when no device is visible. */
void ScrnCommon_EmptyControlInit(void)
{
    createWindow();
    ScrnCommon_StatusBarCreate();
    createText(&textbox_6, 5, 0, 20, 127, 37, "Device control", C_DARK_SLATE_BLUE);
    createText(&textbox_7, 6, 0, 48, 127, 65, "No visible devices", C_BLACK);
    createText(&textbox_8, 8, 0, 76, 127, 93, "Use Device manager", C_BLACK);
    createText(&textbox_9, 10, 0, 94, 127, 111, "to show devices", C_BLACK);
    createText(&textbox_4, 3, 0, 144, 127, 159, "Device manager", C_RED);
    UG_WindowShow(&window_1);
    UG_Update();
}

/** @brief Refresh the empty device control screen. */
void ScrnCommon_EmptyControlRender(void)
{
    ScrnCommon_StatusBarUpdate();
    UG_Update();
}

/**
 * @brief  Let only the selected row of a main-style menu scroll its text.
 * @param  count     Number of rows.
 * @param  selected  Selected row.
 */
void ScrnCommon_MenuScrollUpdate(uint8_t count, uint8_t selected)
{
    for (uint8_t i = 0; i < count; i++)
    {
        UG_TextboxSetScrollEnabled(&window_1, TXB_ID_6 + i * 2, i == selected);
        UG_TextboxSetScrollEnabled(&window_1, TXB_ID_7 + i * 2, i == selected);
    }
}

/**
 * @brief  Replace the value textbox with one box per character for editing.
 * @param  count  Number of characters (at most 19, in rows of 10).
 */
void ScrnCommon_EditCharactersCreate(uint8_t count)
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

/**
 * @brief  Copy the value text into the character boxes and highlight the cursor.
 * @param  position  Character under the cursor.
 */
void ScrnCommon_EditCharactersUpdate(uint8_t position)
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
    ScrnCommon_StatusBarCreate();
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

/* Flat position of the selection: device * MAXIMUM_MODULE + module. */
static int selectionPosition(void)
{
    if (systemInfo.currentDeviceId >= MAXIMUM_DEVICE || systemInfo.currentModuleId >= MAXIMUM_MODULE)
    {
        return -1;
    }
    return systemInfo.currentDeviceId * MAXIMUM_MODULE + systemInfo.currentModuleId;
}

/* Select the motor at a flat position; FALSE (nothing changed) for a negative position. */
static uint8_t selectPosition(int position)
{
    if (position < 0)
    {
        return 0;
    }
    systemInfo.currentDeviceId = position / MAXIMUM_MODULE;
    systemInfo.currentModuleId = position % MAXIMUM_MODULE;
    return 1;
}

/*
 * Flat position of the visible motor closest to position: the smallest one
 * after it (direction +1) or the largest one before it (-1); -1 if none.
 * Scans the motor pool, so a step costs MAXIMUM_MOTOR lookups.
 */
static int nearestVisible(int position, int8_t direction)
{
    int best = -1;
    int candidate;

    for (uint8_t slot = 0; slot < MAXIMUM_MOTOR; slot++)
    {
        const MotorStorageData_t *motor = &storageData.MotorConfig[slot];

        if (motor->device >= MAXIMUM_DEVICE || motor->module >= MAXIMUM_MODULE ||
            storageData.DeviceConfig[motor->device].deleted || motor->hidden)
        {
            continue;
        }
        candidate = motor->device * MAXIMUM_MODULE + motor->module;
        if (direction > 0 ? candidate > position && (best < 0 || candidate < best) :
                            candidate < position && (best < 0 || candidate > best))
        {
            best = candidate;
        }
    }
    return best;
}
