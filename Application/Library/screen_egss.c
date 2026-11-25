#include "link_monitor.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"
#include "lora.h"

/* Items of the general settings menu (indexOfGeneralSettingMenu). */
#define ITEM_SLEEP_MODE     0
#define ITEM_SCAN_INTERVAL  1
#define ITEM_SLEEP_CHECK    2
#define ITEM_NET_ID         3
#define ITEM_FREQUENCY      4
#define ITEM_SPREAD_FACTOR  5
#define ITEM_SLEEP_AFTER    6
#define ITEM_LINK_TIMEOUT   7

#define DIRECTION_UP   0
#define DIRECTION_DOWN 1

#define SF_OPTIONS         6
#define MIN_FREQUENCY_MHZ  137
#define TXB_ID_DESCRIPTION 60
#define TXB_ID_HELP        61

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

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
static uint8_t hasCharacterEditor(void);
static void createLabel(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye, char *text,
    UG_COLOR background, uint8_t alignment);
static void createButtons(char *left, char *center, char *right);
static void createCaption(char *text);
static void createValue(char *text);
static void createSpreadFactorList(void);
static uint8_t parseDigits(const char *text, uint8_t length, uint8_t digits, int *value);
static void stepDigits(uint8_t length, uint8_t digits, const char *format, uint8_t step);
static char stepHexCharacter(char c, uint8_t direction);
static void stepNetId(uint8_t direction);
static void moveSpreadFactor(uint8_t direction);
static void adjust(uint8_t direction);
static void back(void);

/* LEFT/RIGHT: inc/dec (or move the selection), CENTER: next digit (or select).
   Hold CENTER: save, hold RIGHT: cancel. */
Screen_t editGeneralSettingScreen = {
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

/* Screen handlers ========================================================== */

static void init(void)
{
    const uint8_t counts[] = {0, 6, 6, 2, 7, 0, 6, 6};
    const char *descriptions[] = {
        "Enable idle sleep\nRTC/LoRa can wake",
        "Local link scan (s)\nNo periodic polling",
        "Sleep RTC wake (s)\nCheck silent devices",
        "LoRa network ID\n01-FE hex",
        "Radio frequency (MHz)\n137-999 in editor",
        "LoRa spreading factor",
        "Idle before sleep (s)\nNo button activity",
        "Silence threshold (s)\nThen verify status"
    };
    char text[20];

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
    createLabel(&textbox_6, TXB_ID_5, 0, 20, LCD_WIDTH - 1, 37, "General setting", C_DARK_SLATE_BLUE,
        ALIGN_CENTER_LEFT);

    /* Editor for the selected item. */
    if (indexOfGeneralSettingMenu == ITEM_SLEEP_MODE)
    {
        createButtons(" Up", "Select", "Down ");
        createCaption("Enable sleep mode");
        createLabel(&textbox_8, TXB_ID_7, 15, 57, 120, 67, "Yes", C_BLACK, ALIGN_CENTER);
        createLabel(&textbox_9, TXB_ID_8, 15, 70, 120, 80, "No", C_BLACK, ALIGN_CENTER);
        if (systemInfo.sleepMode == 1)
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
    else if (isSecondsItem())
    {
        createButtons(" Inc", "Next", "Dec ");
        createCaption(indexOfGeneralSettingMenu == ITEM_SLEEP_AFTER ? "Sleep after idle" :
            indexOfGeneralSettingMenu == ITEM_LINK_TIMEOUT ? "Link timeout" :
            indexOfGeneralSettingMenu == ITEM_SLEEP_CHECK ? "Sleep check interval" : "Link scan interval");
        if (indexOfGeneralSettingMenu == ITEM_SLEEP_CHECK)
        {
            sprintf(text, "%04d s", systemInfo.tSleepUpdate);
        }
        else
        {
            sprintf(text, "%04d s", indexOfGeneralSettingMenu == ITEM_SLEEP_AFTER ? Link_GetIdleSeconds() :
                indexOfGeneralSettingMenu == ITEM_LINK_TIMEOUT ? Link_GetTimeoutSeconds() : systemInfo.tUpdate);
        }
        createValue(text);
        indexOfEdittingValue = 0;
    }
    else if (indexOfGeneralSettingMenu == ITEM_NET_ID)
    {
        createButtons(" Inc", "Next", "Dec ");
        createCaption("LoRa network ID");
        sprintf(text, "%02X", systemInfo.netIdConfig);
        createValue(text);
        indexOfEdittingValue = 0;
    }
    else if (indexOfGeneralSettingMenu == ITEM_FREQUENCY)
    {
        createButtons(" Inc", "Next", "Dec ");
        createCaption("LoRa frequency (MHz)");
        sprintf(text, "%03d Mhz", systemInfo.frequencyConfig);
        createValue(text);
        indexOfEdittingValue = 0;
    }
    else if (indexOfGeneralSettingMenu == ITEM_SPREAD_FACTOR)
    {
        createButtons(" Up", "Select", "Down ");
        createCaption("LoRa spreading factor");
        createSpreadFactorList();
    }

    /* One box per character for the numeric editors. */
    if (hasCharacterEditor())
    {
        ScrnCommon_EditCharactersCreate(counts[indexOfGeneralSettingMenu]);
        ScrnCommon_EditCharactersUpdate(indexOfEdittingValue);
    }

    /* Description and help (no room below the SF list). */
    if (indexOfGeneralSettingMenu != ITEM_SPREAD_FACTOR)
    {
        createLabel(&settingDescription, TXB_ID_DESCRIPTION, 0, 100, 127, 120,
            (char *)descriptions[indexOfGeneralSettingMenu], C_BLACK, ALIGN_CENTER_LEFT);
        UG_TextboxSetScrollEnabled(&window_1, TXB_ID_DESCRIPTION, 0);
        createLabel(&settingHelp, TXB_ID_HELP, 0, 123, 127, 141, "Hold CENTER: save\nHold RIGHT: cancel",
            C_BLACK, ALIGN_CENTER_LEFT);
        UG_TextboxSetScrollEnabled(&window_1, TXB_ID_HELP, 0);
    }
    UG_WindowShow(&window_1);
    UG_Update();
}

static void render(void)
{
    if (hasCharacterEditor())
    {
        ScrnCommon_EditCharactersUpdate(indexOfEdittingValue);
    }
    if (indexOfGeneralSettingMenu == ITEM_SPREAD_FACTOR)
    {
        for (uint8_t i = 0; i < SF_OPTIONS; i++)
        {
            UG_TextboxSetBackColor(&window_1, TXB_ID_7 + i, i == indexOfSelection ? C_DIM_GRAY : C_BLACK);
        }
    }
    ScrnCommon_StatusBarUpdate();
    UG_Update();
}

static void leftClicked(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    adjust(DIRECTION_UP);
}

static void rightClicked(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    adjust(DIRECTION_DOWN);
}

/* Move the cursor; for selection items apply the choice and leave. */
static void centerClicked(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (indexOfGeneralSettingMenu == ITEM_SLEEP_MODE)
    {
        systemInfo.sleepMode = (indexOfSelection + 1) % 2;
    }
    else if (isSecondsItem())
    {
        indexOfEdittingValue = (indexOfEdittingValue + 1) % 4;
    }
    else if (indexOfGeneralSettingMenu == ITEM_NET_ID)
    {
        indexOfEdittingValue = (indexOfEdittingValue + 1) % 2;
    }
    else if (indexOfGeneralSettingMenu == ITEM_FREQUENCY)
    {
        indexOfEdittingValue = (indexOfEdittingValue + 1) % 3;
    }
    else if (indexOfGeneralSettingMenu == ITEM_SPREAD_FACTOR)
    {
        systemInfo.sfConfig = indexOfSelection + SF_7;
    }

    if (indexOfGeneralSettingMenu == ITEM_SLEEP_MODE || indexOfGeneralSettingMenu == ITEM_SPREAD_FACTOR)
    {
        ScrnCommon_SaveGeneralSettings();
        back();
    }
}

/* Cancel. */
static void rightPressed(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    back();
}

/* Validate and save; an invalid value beeps long and stays in the editor. */
static void centerPressed(void)
{
    char *valueText;
    char *endPtr;
    long net;
    int value;

    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (indexOfGeneralSettingMenu == ITEM_SLEEP_MODE)
    {
        systemInfo.sleepMode = (indexOfSelection + 1) % 2;
    }
    else if (indexOfGeneralSettingMenu == ITEM_SCAN_INTERVAL || indexOfGeneralSettingMenu == ITEM_SLEEP_CHECK)
    {
        /* A malformed text keeps the old value; zero is rejected. */
        valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
        if (parseDigits(valueText, 6, 4, &value))
        {
            if (value < 1)
            {
                free(valueText);
                Main_SetBuzzerMode(BUZZER_LONG_BEEP);
                return;
            }
            if (indexOfGeneralSettingMenu == ITEM_SCAN_INTERVAL)
            {
                systemInfo.tUpdate = value;
            }
            else
            {
                systemInfo.tSleepUpdate = value;
            }
        }
        free(valueText);
    }
    else if (indexOfGeneralSettingMenu == ITEM_NET_ID)
    {
        valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
        net = strtol(valueText, &endPtr, 16);
        if (*endPtr || net <= 0 || net >= 255)
        {
            free(valueText);
            Main_SetBuzzerMode(BUZZER_LONG_BEEP);
            return;
        }
        systemInfo.netIdConfig = net;
        free(valueText);
    }
    else if (indexOfGeneralSettingMenu == ITEM_FREQUENCY)
    {
        valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
        if (parseDigits(valueText, 7, 3, &value))
        {
            if (value < MIN_FREQUENCY_MHZ)
            {
                free(valueText);
                Main_SetBuzzerMode(BUZZER_LONG_BEEP);
                return;
            }
            systemInfo.frequencyConfig = value;
        }
        free(valueText);
    }
    else if (indexOfGeneralSettingMenu == ITEM_SPREAD_FACTOR)
    {
        systemInfo.sfConfig = indexOfSelection + SF_7;
    }
    else if (indexOfGeneralSettingMenu == ITEM_SLEEP_AFTER || indexOfGeneralSettingMenu == ITEM_LINK_TIMEOUT)
    {
        /* Here a malformed text is rejected as well. */
        valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
        if (!parseDigits(valueText, 6, 4, &value))
        {
            value = 0;
        }
        free(valueText);
        if (!value)
        {
            Main_SetBuzzerMode(BUZZER_LONG_BEEP);
            return;
        }
        if (indexOfGeneralSettingMenu == ITEM_SLEEP_AFTER)
        {
            Link_SetIdleSeconds(value);
        }
        else
        {
            Link_SetTimeoutSeconds(value);
        }
    }

    ScrnCommon_SaveGeneralSettings();
    back();
}

static void noAction(void)
{
}

/* Private helpers: widgets ================================================= */

static void windowCallback(UG_MESSAGE *message)
{
    (void)message;
}

/* Items edited as a four-digit number of seconds. */
static uint8_t isSecondsItem(void)
{
    return indexOfGeneralSettingMenu == ITEM_SCAN_INTERVAL || indexOfGeneralSettingMenu == ITEM_SLEEP_CHECK ||
        indexOfGeneralSettingMenu == ITEM_SLEEP_AFTER || indexOfGeneralSettingMenu == ITEM_LINK_TIMEOUT;
}

static uint8_t hasCharacterEditor(void)
{
    return (indexOfGeneralSettingMenu >= 1 && indexOfGeneralSettingMenu <= 4) || indexOfGeneralSettingMenu >= 6;
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

static void createValue(char *text)
{
    createLabel(&textbox_8, TXB_ID_7, 15, 57, 120, 67, text, C_BLACK, ALIGN_CENTER);
}

/* SF7..SF12, one row each, with the current setting selected. */
static void createSpreadFactorList(void)
{
    UG_TEXTBOX *boxes[] = {&textbox_8, &textbox_9, &textbox_10, &textbox_11, &textbox_12, &textbox_13};
    char *names[] = {"SF7", "SF8", "SF9", "SF10", "SF11", "SF12"};
    int y;

    for (uint8_t i = 0; i < SF_OPTIONS; i++)
    {
        y = 57 + i * 13;
        createLabel(boxes[i], TXB_ID_7 + i, 15, y, 120, y + 10, names[i], C_BLACK, ALIGN_CENTER);
    }
    if (systemInfo.sfConfig >= SF_7 && systemInfo.sfConfig <= SF_12)
    {
        UG_TextboxSetBackColor(&window_1, systemInfo.sfConfig - SF_7 + TXB_ID_7, C_DIM_GRAY);
        indexOfSelection = systemInfo.sfConfig - SF_7;
    }
    else
    {
        UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_DIM_GRAY);
        indexOfSelection = 0;
    }
}

/* Private helpers: editing ================================================= */

/* Text of exactly length characters starting with digits decimal digits -> value. */
static uint8_t parseDigits(const char *text, uint8_t length, uint8_t digits, int *value)
{
    *value = 0;
    if (strlen(text) != length)
    {
        return 0;
    }
    for (uint8_t i = 0; i < digits; i++)
    {
        if (text[i] < '0' || text[i] > '9')
        {
            return 0;
        }
        *value = *value * 10 + text[i] - '0';
    }
    return 1;
}

/* Add step (1 = up, 9 = down) modulo 10 to the digit under the cursor and reformat. */
static void stepDigits(uint8_t length, uint8_t digits, const char *format, uint8_t step)
{
    char *valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
    char newValueText[20];
    uint8_t digit[4];
    int value;

    if (parseDigits(valueText, length, digits, &value))
    {
        for (uint8_t i = 0; i < digits; i++)
        {
            digit[i] = valueText[i] - '0';
        }
        if (indexOfEdittingValue < digits)
        {
            digit[indexOfEdittingValue] = (digit[indexOfEdittingValue] + step) % 10;
        }
        value = 0;
        for (uint8_t i = 0; i < digits; i++)
        {
            value = value * 10 + digit[i];
        }
        sprintf(newValueText, format, value);
        UG_TextboxSetText(&window_1, TXB_ID_7, newValueText);
        UG_Update();
    }
    free(valueText);
}

/* Hex digits cycle 0-9, A-F. */
static char stepHexCharacter(char c, uint8_t direction)
{
    if (direction == DIRECTION_UP)
    {
        if (c == '9')
        {
            return 'A';
        }
        if (c == 'F')
        {
            return '0';
        }
        if ((c >= '0' && c < '9') || (c >= 'A' && c < 'F'))
        {
            return c + 1;
        }
        return '0';
    }
    if (c == '0')
    {
        return 'F';
    }
    if (c == 'A')
    {
        return '9';
    }
    if ((c > '0' && c <= '9') || (c > 'A' && c <= 'F'))
    {
        return c - 1;
    }
    return '0';
}

static void stepNetId(uint8_t direction)
{
    char *valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
    char newValue[20];

    strncpy(newValue, valueText, sizeof(newValue) - 1);
    newValue[sizeof(newValue) - 1] = '\0';
    if (indexOfEdittingValue < sizeof(newValue) - 1)
    {
        newValue[indexOfEdittingValue] = stepHexCharacter(newValue[indexOfEdittingValue], direction);
    }
    UG_TextboxSetText(&window_1, TXB_ID_7, newValue);
    UG_Update();
    free(valueText);
}

/* Move the SF selection and grey it, clearing its neighbours. */
static void moveSpreadFactor(uint8_t direction)
{
    uint8_t id;

    if ((direction == DIRECTION_UP && indexOfSelection > 0) ||
        (direction == DIRECTION_DOWN && indexOfSelection < SF_OPTIONS - 1))
    {
        if (direction == DIRECTION_UP)
        {
            indexOfSelection--;
        }
        else
        {
            indexOfSelection++;
        }
        id = TXB_ID_7 + indexOfSelection;
        UG_TextboxSetBackColor(&window_1, id, C_DIM_GRAY);
        if (id < TXB_ID_12)
        {
            UG_TextboxSetBackColor(&window_1, id + 1, C_BLACK);
        }
        if (id > TXB_ID_7)
        {
            UG_TextboxSetBackColor(&window_1, id - 1, C_BLACK);
        }
        UG_Update();
    }
}

/* LEFT (up) and RIGHT (down) do the same thing in opposite directions. */
static void adjust(uint8_t direction)
{
    uint8_t step = direction == DIRECTION_UP ? 1 : 9;

    if (indexOfGeneralSettingMenu == ITEM_SLEEP_MODE)
    {
        if (direction == DIRECTION_UP && indexOfSelection == 1)
        {
            UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_DIM_GRAY);
            UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
            indexOfSelection = 0;
        }
        else if (direction == DIRECTION_DOWN && indexOfSelection == 0)
        {
            UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
            UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_DIM_GRAY);
            indexOfSelection = 1;
        }
    }
    else if (isSecondsItem())
    {
        stepDigits(6, 4, "%04d s", step);
    }
    else if (indexOfGeneralSettingMenu == ITEM_NET_ID)
    {
        stepNetId(direction);
    }
    else if (indexOfGeneralSettingMenu == ITEM_FREQUENCY)
    {
        stepDigits(7, 3, "%03d Mhz", step);
    }
    else if (indexOfGeneralSettingMenu == ITEM_SPREAD_FACTOR)
    {
        moveSpreadFactor(direction);
    }
}

static void back(void)
{
    UG_WindowHide(&window_1);
    UG_WindowDelete(&window_1);
    currentScreen = &generalSettingScreen;
    currentScreen->init();
    currentScreen->render();
}
