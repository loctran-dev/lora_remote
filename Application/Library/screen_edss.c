#include "link_monitor.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"
#include "config_store.h"
#include "command_handlers.h"

/* Items of the device settings menu (indexOfSettingMenu). */
#define ITEM_ALARM_THRESHOLD 0
#define ITEM_ALARM_DELAY     1
#define ITEM_LOCK            2
#define ITEM_HIDE            3
#define ITEM_DELETE          4
#define ITEM_NAME            5
#define ITEM_AUTO_STATUS     6

#define SECONDS_DIGITS 4
#define NAME_LENGTH    19
#define TXB_ID_DESCRIPTION 60
#define TXB_ID_HELP        61

extern SystemInfo_t systemInfo;

static uint8_t indexOfEdittingValue;
static uint8_t indexOfSelection;
static UG_TEXTBOX settingDescription;
static UG_TEXTBOX settingHelp;

static void init(void);
static void render(void);
static void leftClicked(void);
static void rightClicked(void);
static void centerClicked(void);
static void rightPressed(void);
static void centerPressed(void);
static void noAction(void);
static void windowCallback(UG_MESSAGE *message);
static uint8_t isSecondsItem(void);
static MotorStorageData_t *currentConfig(void);
static void createLabel(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye, char *text,
    UG_COLOR background, uint8_t alignment);
static void createButtons(char *left, char *center, char *right);
static void createCaption(char *text);
static void createYesNo(char *caption, uint8_t yesSelected);
static void createAlarmThresholdEditor(float value);
static void alarmThresholdCursorUpdate(void);
static void adjustAlarmThreshold(uint8_t increment);
static uint8_t parseSeconds(const char *text, int *value);
static void stepSeconds(uint8_t step);
static char nextNameCharacter(char c);
static char previousNameCharacter(char c);
static void stepNameCharacter(uint8_t up);
static void selectYesNo(uint8_t index);
static uint8_t applySelection(void);
static uint8_t isMotorItem(uint8_t item);

/* LEFT/RIGHT: inc/dec (or Yes/No), CENTER: next digit (or select).
   Hold CENTER: save, hold RIGHT: cancel. */
Screen_t editDeviceSettingScreen = {
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
    centerPressed,
    noAction,
    noAction
};

static void init(void)
{
    const char *descriptions[] = {
        "I alarm threshold (A)\nLow current limit",
        "Low current duration\nbefore raising alarm",
        "Block remote ON/OFF\nRemote commands only",
        "Hide from control UI\nUse manager to show",
        "Unpair on this remote\nHistory is retained",
        "Device display name\nEdit one letter/box",
        "Status interval (s)\n0 = status disabled"
    };
    MotorStorageData_t module;
    char title[40];
    char secondsText[20];

    /* Window, status bar and title. */
    UG_WindowCreate(&window_1, obj_buff_wnd_1, MAX_OBJECTS, windowCallback);
    UG_WindowSetTitleText(&window_1, "");
    UG_WindowSetTitleTextFont(&window_1, DEFAULT_FONT);
    UG_WindowSetTitleHeight(&window_1, 0);
    UG_WindowSetStyle(&window_1, WND_STYLE_HIDE_TITLE);
    UG_WindowSetBackColor(&window_1, C_BLACK);
    UG_WindowSetXStart(&window_1, 0);
    UG_WindowSetYStart(&window_1, 0);
    UG_WindowSetXEnd(&window_1, LCD_WIDTH - 1);
    UG_WindowSetYEnd(&window_1, LCD_HEIGHT - 1);
    ScrnCommon_StatusBarCreate();
    module = *ScrnCommon_CurrentMotor();
    snprintf(title, sizeof(title), " %c%u%u.%.19s-Set.",
        storageData.devices[systemInfo.curDevice].factoryInfo.typeChar, systemInfo.curDevice + 1,
        systemInfo.curModule + 1, module.name);
    createLabel(&textbox_6, TXB_ID_5, 0, 20, 127, 37, title, C_DARK_SLATE_BLUE, ALIGN_CENTER_LEFT);

    /* Editor for the selected item. */
    if (indexOfSettingMenu == ITEM_ALARM_THRESHOLD)
    {
        createButtons(" Inc", "Next", "Dec ");
        createCaption("Low-current alarm");
        createAlarmThresholdEditor(module.alarmThreshold);
    }
    else if (isSecondsItem())
    {
        createButtons(" Inc", "Next", "Dec ");
        createCaption(indexOfSettingMenu == ITEM_AUTO_STATUS ? "Auto status interval" : "Alarm delay");
        sprintf(secondsText, "%04d s", indexOfSettingMenu == ITEM_AUTO_STATUS ?
            Link_GetAutoStatus(systemInfo.curDevice) : module.alarmDelay);
        createLabel(&textbox_8, TXB_ID_7, 15, 57, 120, 67, secondsText, C_BLACK, ALIGN_CENTER);
        indexOfEdittingValue = 0;
    }
    else if (indexOfSettingMenu == ITEM_LOCK)
    {
        createButtons("Up", "Select", "Down ");
        createYesNo("Lock", module.locked == 1);
    }
    else if (indexOfSettingMenu == ITEM_HIDE)
    {
        createButtons(" Up", "Select", "Down ");
        createYesNo("Hide", module.hidden == 1);
    }
    else if (indexOfSettingMenu == ITEM_DELETE)
    {
        createButtons(" Up", "Select", "Down ");
        createYesNo("Delete", 0);
    }
    else if (indexOfSettingMenu == ITEM_NAME)
    {
        createButtons(" Inc", "Next", "Dec ");
        createCaption("Device name");
        createLabel(&textbox_8, TXB_ID_7, 15, 57, 120, 67, module.name, C_BLACK, ALIGN_CENTER);
        indexOfEdittingValue = 0;
    }

    /* One box per character for the seconds and name editors. */
    if (isSecondsItem() || indexOfSettingMenu == ITEM_NAME)
    {
        ScrnCommon_EditCharactersCreate(isSecondsItem() ? 6 : NAME_LENGTH);
        ScrnCommon_EditCharactersUpdate(indexOfEdittingValue);
    }

    /* Description and help. */
    createLabel(&settingDescription, TXB_ID_DESCRIPTION, 0, 100, 127, 120,
        (char *)descriptions[indexOfSettingMenu], C_BLACK, ALIGN_CENTER_LEFT);
    UG_TextboxSetScrollEnabled(&window_1, TXB_ID_DESCRIPTION, 0);
    if (indexOfSettingMenu == ITEM_ALARM_THRESHOLD)
    {
        UG_TextboxHide(&window_1, TXB_ID_8);
    }
    createLabel(&settingHelp, TXB_ID_HELP, 0, 123, 127, 141, "Hold CENTER: save\nHold RIGHT: cancel",
        C_BLACK, ALIGN_CENTER_LEFT);
    UG_TextboxSetScrollEnabled(&window_1, TXB_ID_HELP, 0);
    UG_WindowShow(&window_1);
    UG_Update();
}

static void render(void)
{
    if (isSecondsItem() || indexOfSettingMenu == ITEM_NAME)
    {
        ScrnCommon_EditCharactersUpdate(indexOfEdittingValue);
    }
    ScrnCommon_StatusBarUpdate();
    UG_Update();
}

/* Increment the value under the cursor, or select "Yes". */
static void leftClicked(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    if (indexOfSettingMenu == ITEM_ALARM_THRESHOLD)
    {
        adjustAlarmThreshold(1);
    }
    else if (isSecondsItem())
    {
        stepSeconds(1);
    }
    else if (indexOfSettingMenu == ITEM_LOCK || indexOfSettingMenu == ITEM_HIDE || indexOfSettingMenu == ITEM_DELETE)
    {
        if (indexOfSelection == 1)
        {
            selectYesNo(0);
            UG_Update();
        }
    }
    else if (indexOfSettingMenu == ITEM_NAME)
    {
        stepNameCharacter(1);
    }
}

/* Decrement the value under the cursor, or select "No". */
static void rightClicked(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    if (indexOfSettingMenu == ITEM_ALARM_THRESHOLD)
    {
        adjustAlarmThreshold(0);
    }
    else if (isSecondsItem())
    {
        stepSeconds(9);
    }
    else if (indexOfSettingMenu == ITEM_LOCK || indexOfSettingMenu == ITEM_HIDE || indexOfSettingMenu == ITEM_DELETE)
    {
        if (indexOfSelection == 0)
        {
            selectYesNo(1);
        }
    }
    else if (indexOfSettingMenu == ITEM_NAME)
    {
        stepNameCharacter(0);
    }
}

/* Move the cursor; for Yes/No items apply the selection and leave. */
static void centerClicked(void)
{
    uint8_t changed = 0;

    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    if (indexOfSettingMenu == ITEM_ALARM_THRESHOLD || isSecondsItem())
    {
        indexOfEdittingValue = (indexOfEdittingValue + 1) % SECONDS_DIGITS;
        if (indexOfSettingMenu == ITEM_ALARM_THRESHOLD)
        {
            alarmThresholdCursorUpdate();
        }
    }
    else if (indexOfSettingMenu == ITEM_NAME)
    {
        indexOfEdittingValue = (indexOfEdittingValue + 1) % NAME_LENGTH;
    }
    else
    {
        if (!ScrnEdss_CanEdit(indexOfSettingMenu))
        {
            Main_SetBuzzerMode(eBuzzer_Mode_LongBeep);
            ScrnCommon_Switch(&deviceSettingScreen);
            return;
        }
        changed = applySelection();
    }

    /* Flash is rewritten only when the flag really changed (KI-7). */
    if (indexOfSettingMenu == ITEM_LOCK || indexOfSettingMenu == ITEM_HIDE)
    {
        if (changed)
        {
            ScrnCommon_SaveDeviceSettings();
        }
        ScrnCommon_Switch(indexOfSettingMenu == ITEM_HIDE && currentConfig()->hidden ?
            &deviceControlScreen : &deviceSettingScreen);
    }
    else if (indexOfSettingMenu == ITEM_DELETE)
    {
        if (changed)
        {
            ScrnCommon_SaveDeviceSettings();
        }
        ScrnCommon_Switch(&deviceControlScreen);
    }
}

/* Cancel. */
static void rightPressed(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    ScrnCommon_Switch(&deviceSettingScreen);
}

/* Save the edited value, store it and send the module config when needed. */
static void centerPressed(void)
{
    char *valueText;
    char nameText[20];
    int value;
    uint8_t changed = 0;

    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    /* A motor setting without a MotorConfig slot would only reach the placeholder (KI-4). */
    if (!ScrnEdss_CanEdit(indexOfSettingMenu))
    {
        Main_SetBuzzerMode(eBuzzer_Mode_LongBeep);
        ScrnCommon_Switch(&deviceSettingScreen);
        return;
    }

    /* Copy the edited value into the device data. */
    if (indexOfSettingMenu == ITEM_ALARM_THRESHOLD)
    {
        currentConfig()->alarmThreshold = UG_RealEditGetValue(&window_1, REDT_ID_0);
    }
    else if (isSecondsItem())
    {
        valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
        if (parseSeconds(valueText, &value))
        {
            if (indexOfSettingMenu == ITEM_AUTO_STATUS)
            {
                Link_SetAutoStatus(systemInfo.curDevice, value);
            }
            else
            {
                currentConfig()->alarmDelay = value;
            }
        }
        free(valueText);
    }
    else if (indexOfSettingMenu == ITEM_NAME)
    {
        valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
        strncpy(nameText, valueText, sizeof(nameText) - 1);
        nameText[sizeof(nameText) - 1] = '\0';
        for (int end = strlen(nameText); end > 0 && nameText[end - 1] == ' '; end--)
        {
            nameText[end - 1] = '\0';
        }
        free(valueText);
        strncpy(currentConfig()->name, nameText, sizeof(currentConfig()->name) - 1);
        currentConfig()->name[sizeof(currentConfig()->name) - 1] = '\0';
    }
    else
    {
        changed = applySelection();
    }

    /* Persist; threshold and delay are also sent to the slave. */
    if (indexOfSettingMenu == ITEM_ALARM_THRESHOLD || isSecondsItem())
    {
        ScrnCommon_SaveDeviceSettings();
        if (!CmdHandler_SendCurrentModuleConfig())
        {
            Main_SetBuzzerMode(eBuzzer_Mode_LongBeep);
        }
    }
    if ((indexOfSettingMenu == ITEM_LOCK || indexOfSettingMenu == ITEM_HIDE ||
         indexOfSettingMenu == ITEM_DELETE) && changed)
    {
        ScrnCommon_SaveDeviceSettings();
    }
    if (indexOfSettingMenu == ITEM_NAME)
    {
        ScrnCommon_SaveDeviceSettings();
    }

    /* A hidden or deleted device leaves the settings menu. */
    if (indexOfSettingMenu != ITEM_DELETE)
    {
        ScrnCommon_Switch(indexOfSettingMenu == ITEM_HIDE && currentConfig()->hidden ?
            &deviceControlScreen : &deviceSettingScreen);
    }
    else
    {
        ScrnCommon_Switch(&deviceControlScreen);
    }
}

static void noAction(void)
{
}

static void windowCallback(UG_MESSAGE *message)
{
    (void)message;
}

/* Items 1 and 6 edit a four-digit number of seconds. */
static uint8_t isSecondsItem(void)
{
    return indexOfSettingMenu == ITEM_ALARM_DELAY || indexOfSettingMenu == ITEM_AUTO_STATUS;
}

static MotorStorageData_t *currentConfig(void)
{
    return ScrnCommon_CurrentMotor();
}

static void createLabel(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye, char *text,
    UG_COLOR background, uint8_t alignment)
{
    UG_TextboxCreate(&window_1, box, id, x, y, xe, ye);
    UG_TextboxSetFont(&window_1, id, DEFAULT_FONT);
    UG_TextboxSetText(&window_1, id, text);
    UG_TextboxSetBackColor(&window_1, id, background);
    UG_TextboxSetForeColor(&window_1, id, C_WHITE);
    UG_TextboxSetAlignment(&window_1, id, alignment);
}

static void createButtons(char *left, char *center, char *right)
{
    createLabel(&textbox_3, TXB_ID_2, 0, 144, 43, 159, left, C_RED, ALIGN_CENTER_LEFT);
    createLabel(&textbox_4, TXB_ID_3, 44, 144, 83, 159, center, C_RED, ALIGN_CENTER);
    createLabel(&textbox_5, TXB_ID_4, 84, 144, 127, 159, right, C_RED, ALIGN_CENTER_RIGHT);
}

static void createCaption(char *text)
{
    createLabel(&textbox_7, TXB_ID_6, 15, 42, LCD_WIDTH - 1, 52, text, C_BLACK, ALIGN_CENTER_LEFT);
}

static void createYesNo(char *caption, uint8_t yesSelected)
{
    createCaption(caption);
    createLabel(&textbox_8, TXB_ID_7, 15, 57, 120, 67, "Yes", C_BLACK, ALIGN_CENTER);
    createLabel(&textbox_9, TXB_ID_8, 15, 70, 120, 80, "No", C_BLACK, ALIGN_CENTER);
    if (yesSelected)
    {
        UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_DIM_GRAY);
        indexOfSelection = 0;
    }
    else
    {
        UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_DIM_GRAY);
        indexOfSelection = 1;
    }
}

static void createAlarmThresholdEditor(float value)
{
    UG_RealEditCreate(&window_1, &real_edit_1, REDT_ID_0, 28, 57, 97, 73, 5, 2);
    UG_RealEditSetFont(&window_1, REDT_ID_0, DEFAULT_FONT);
    UG_RealEditSetRange(&window_1, REDT_ID_0, 0.0f, 99.99f);
    UG_RealEditSetValue(&window_1, REDT_ID_0, value);
    UG_RealEditSetBackColor(&window_1, REDT_ID_0, C_BLACK);
    UG_RealEditSetForeColor(&window_1, REDT_ID_0, C_WHITE);
    UG_RealEditSetCursorBackColor(&window_1, REDT_ID_0, C_YELLOW);
    UG_RealEditSetCursorForeColor(&window_1, REDT_ID_0, C_BLACK);
    UG_RealEditSetAlignment(&window_1, REDT_ID_0, ALIGN_CENTER);
    UG_RealEditSetCharWidth(&window_1, REDT_ID_0, 14);
    indexOfEdittingValue = 0;
    UG_RealEditSetCursorPosition(&window_1, REDT_ID_0, 0);
    createLabel(&textbox_8, TXB_ID_7, 101, 57, 113, 73, "A", C_BLACK, ALIGN_CENTER);
    createLabel(&textbox_9, TXB_ID_8, 0, 115, 127, 129, "Hold: Save / Back", C_BLACK, ALIGN_CENTER);
}

/* Map digit index 0..3 to the character position in "dd.dd". */
static void alarmThresholdCursorUpdate(void)
{
    const uint8_t positions[] = {0, 1, 3, 4};

    UG_RealEditSetCursorPosition(&window_1, REDT_ID_0, positions[indexOfEdittingValue]);
    UG_Update();
}

static void adjustAlarmThreshold(uint8_t increment)
{
    const uint16_t weights[] = {1000, 100, 10, 1};
    uint16_t value = (uint16_t)(UG_RealEditGetValue(&window_1, REDT_ID_0) * 100.0f + 0.5f);
    uint16_t weight = weights[indexOfEdittingValue];
    uint8_t digit = (value / weight) % 10;
    uint8_t next = (digit + (increment ? 1 : 9)) % 10;

    value = value - digit * weight + next * weight;
    UG_RealEditSetValue(&window_1, REDT_ID_0, (float)value / 100.0f);
    UG_Update();
}

/* "dddd s" -> value; returns 0 if the text is not in that form. */
static uint8_t parseSeconds(const char *text, int *value)
{
    if (strlen(text) != 6)
    {
        return 0;
    }
    for (uint8_t i = 0; i < SECONDS_DIGITS; i++)
    {
        if (text[i] < '0' || text[i] > '9')
        {
            return 0;
        }
    }
    *value = (text[0] - '0') * 1000 + (text[1] - '0') * 100 + (text[2] - '0') * 10 + (text[3] - '0');
    return 1;
}

/* Add step (1 = up, 9 = down) modulo 10 to the digit under the cursor. */
static void stepSeconds(uint8_t step)
{
    char *valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
    char newValueText[20];
    uint8_t digits[SECONDS_DIGITS];
    int value;

    if (parseSeconds(valueText, &value))
    {
        for (uint8_t i = 0; i < SECONDS_DIGITS; i++)
        {
            digits[i] = valueText[i] - '0';
        }
        if (indexOfEdittingValue < SECONDS_DIGITS)
        {
            digits[indexOfEdittingValue] = (digits[indexOfEdittingValue] + step) % 10;
        }
        value = digits[0] * 1000 + digits[1] * 100 + digits[2] * 10 + digits[3];
        sprintf(newValueText, "%04d s", value);
        UG_TextboxSetText(&window_1, TXB_ID_7, newValueText);
        UG_Update();
    }
    free(valueText);
}

/* Name characters cycle 0-9, A-Z, a-z, space. */
static char nextNameCharacter(char c)
{
    if (c == '9')
    {
        return 'A';
    }
    if (c == 'Z')
    {
        return 'a';
    }
    if (c == 'z')
    {
        return ' ';
    }
    if (c == ' ')
    {
        return '0';
    }
    if ((c >= '0' && c < '9') || (c >= 'A' && c < 'Z') || (c >= 'a' && c < 'z'))
    {
        return c + 1;
    }
    return 'A';
}

static char previousNameCharacter(char c)
{
    if (c == '0')
    {
        return ' ';
    }
    if (c == ' ')
    {
        return 'z';
    }
    if (c == 'a')
    {
        return 'Z';
    }
    if (c == 'A')
    {
        return '9';
    }
    if ((c > '0' && c <= '9') || (c > 'A' && c <= 'Z') || (c > 'a' && c <= 'z'))
    {
        return c - 1;
    }
    return 'z';
}

/* Step the name character under the cursor; the name is padded with spaces. */
static void stepNameCharacter(uint8_t up)
{
    char *valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
    char newValue[20];

    memset(newValue, ' ', sizeof(newValue) - 1);
    memcpy(newValue, valueText, strlen(valueText) < sizeof(newValue) - 1 ? strlen(valueText) : sizeof(newValue) - 1);
    newValue[sizeof(newValue) - 1] = '\0';
    if (indexOfEdittingValue < sizeof(newValue) - 1)
    {
        newValue[indexOfEdittingValue] = up ? nextNameCharacter(newValue[indexOfEdittingValue]) :
            previousNameCharacter(newValue[indexOfEdittingValue]);
    }
    UG_TextboxSetText(&window_1, TXB_ID_7, newValue);
    UG_Update();
    free(valueText);
}

/* 0 = Yes, 1 = No. */
static void selectYesNo(uint8_t index)
{
    UG_TextboxSetBackColor(&window_1, TXB_ID_7, index == 0 ? C_DIM_GRAY : C_BLACK);
    UG_TextboxSetBackColor(&window_1, TXB_ID_8, index == 1 ? C_DIM_GRAY : C_BLACK);
    indexOfSelection = index;
}

/* Store Yes/No into the lock, hide or delete flag. */
static uint8_t applySelection(void)
{
    uint8_t value = (indexOfSelection + 1) % 2;
    uint8_t old;

    if (indexOfSettingMenu == ITEM_LOCK)
    {
        old = currentConfig()->locked;
        currentConfig()->locked = value;
    }
    else if (indexOfSettingMenu == ITEM_HIDE)
    {
        old = currentConfig()->hidden;
        currentConfig()->hidden = value;
    }
    else if (indexOfSettingMenu == ITEM_DELETE)
    {
        old = storageData.devices[systemInfo.curDevice].deleted;
        storageData.devices[systemInfo.curDevice].deleted = value;
        if (value)
        {
            Config_FreeMotors(systemInfo.curDevice);
        }
    }
    else
    {
        return 0;
    }
    return old != value;
}

/* Items stored per motor (MotorConfig); delete and auto-status belong to the device. */
static uint8_t isMotorItem(uint8_t item)
{
    return item != ITEM_DELETE && item != ITEM_AUTO_STATUS;
}

/**
 * @brief  Whether a device setting item can be edited for the selected motor.
 * @param  item  Item index (menu index of the device setting screen).
 * @return FALSE for a motor setting of a motor without a MotorConfig slot,
 *         which would only reach a placeholder (KI-4).
 */
uint8_t ScrnEdss_CanEdit(uint8_t item)
{
    return !isMotorItem(item) || Config_Motor(systemInfo.curDevice, systemInfo.curModule) != NULL;
}
