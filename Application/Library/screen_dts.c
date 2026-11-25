#include "screens.h"
#include "main.h"
#include "rtc_setting.h"
#include "utility.h"
#include <stdio.h>

#define FIELD_COUNT 6

extern SystemInfo_t systemInfo;

/* Separate field textboxes: no cursor, no edits to the live RTC until Save. */
static RTC_TimeTypeDef editedTime;
static RTC_DateTypeDef editedDate;
static uint8_t selectedField;
static uint8_t saveError;
static UG_TEXTBOX *boxes[] = {&textbox_1, &textbox_2, &textbox_3, &textbox_4, &textbox_5,
    &textbox_6, &textbox_7, &textbox_8, &textbox_9, &textbox_10, &textbox_11, &textbox_12,
    &textbox_13, &textbox_14, &textbox_15, &textbox_16, &textbox_17, &textbox_18, &textbox_19, &textbox_20};

static void init(void);
static void render(void);
static void leftClicked(void);
static void rightClicked(void);
static void centerClicked(void);
static void rightPressed(void);
static void centerPressed(void);
static void noAction(void);
static void windowCallback(UG_MESSAGE *message);
static uint8_t monthLength(uint8_t year, uint8_t month);
static void createText(uint8_t id, int x, int y, int xe, int ye, char *value, UG_COLOR color);
static void change(int8_t direction);
static void back(void);

/* LEFT +, RIGHT -, CENTER next field; hold CENTER to save, hold RIGHT to cancel. */
Screen_t dateTimeScreen = {
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
    /* Start from the current RTC; an unset clock starts at the first supported year. */
    Rtc_Read();
    editedTime = systemInfo.time;
    editedDate = systemInfo.date;
    if (editedDate.Year < 24)
    {
        editedDate.Year = 24;
    }
    selectedField = saveError = 0;

    /* Window and status bar. */
    UG_WindowCreate(&window_1, obj_buff_wnd_1, MAX_OBJECTS, windowCallback);
    UG_WindowSetStyle(&window_1, WND_STYLE_HIDE_TITLE);
    UG_WindowSetTitleHeight(&window_1, 0);
    UG_WindowSetBackColor(&window_1, C_BLACK);
    UG_WindowSetXStart(&window_1, 0);
    UG_WindowSetYStart(&window_1, 0);
    UG_WindowSetXEnd(&window_1, 127);
    UG_WindowSetYEnd(&window_1, 159);
    ScrnCommon_StatusBarCreate();

    /* Title, field captions, fields and separators. */
    createText(2, 0, 20, 127, 37, "Date & time", C_DARK_SLATE_BLUE);
    createText(3, 0, 41, 127, 51, "DAY    MONTH   YEAR", C_BLACK);
    createText(4, 0, 79, 127, 89, "HOUR    MIN     SEC", C_BLACK);
    for (uint8_t i = 0; i < FIELD_COUNT; i++)
    {
        createText(5 + i, 4 + (i % 3) * 42, i < 3 ? 55 : 93, 4 + (i % 3) * 42 + 31, (i < 3 ? 55 : 93) + 20,
            "", C_BLACK);
    }
    createText(11, 37, 55, 45, 75, "/", C_BLACK);
    createText(12, 79, 55, 87, 75, "/", C_BLACK);
    createText(13, 37, 93, 45, 113, ":", C_BLACK);
    createText(14, 79, 93, 87, 113, ":", C_BLACK);

    /* Help and button labels. */
    createText(15, 0, 117, 127, 127, "Hold CENTER: save", C_BLACK);
    createText(16, 0, 130, 127, 140, "Hold RIGHT: cancel", C_BLACK);
    createText(17, 0, 144, 41, 159, "+", C_RED);
    createText(18, 42, 144, 85, 159, "Next", C_RED);
    createText(19, 86, 144, 127, 159, "-", C_RED);
    UG_WindowShow(&window_1);
}

static void render(void)
{
    char value[24];
    uint16_t values[FIELD_COUNT];

    values[0] = editedDate.Date;
    values[1] = editedDate.Month;
    values[2] = 2000u + editedDate.Year;
    values[3] = editedTime.Hours;
    values[4] = editedTime.Minutes;
    values[5] = editedTime.Seconds;
    for (uint8_t i = 0; i < FIELD_COUNT; i++)
    {
        sprintf(value, i == 2 ? "%04u" : "%02u", values[i]);
        UG_TextboxSetText(&window_1, 5 + i, value);
        UG_TextboxSetBackColor(&window_1, 5 + i, i == selectedField ? C_DARK_SLATE_BLUE : C_BLACK);
        UG_TextboxSetForeColor(&window_1, 5 + i, i == selectedField ? C_WHITE : C_CYAN);
    }

    ScrnCommon_StatusBarUpdate();
    UG_TextboxSetText(&window_1, 15, saveError == eRtc_SetResult_Busy ? "Timer active: blocked" :
        saveError ? "RTC error; try again" : "Hold CENTER: save");
    UG_TextboxSetForeColor(&window_1, 15, saveError ? C_RED : C_WHITE);
    UG_Update();
}

/* Increment the selected field. */
static void leftClicked(void)
{
    change(1);
}

/* Decrement the selected field. */
static void rightClicked(void)
{
    change(-1);
}

/* Select the next field. */
static void centerClicked(void)
{
    selectedField = (selectedField + 1) % FIELD_COUNT;
    saveError = 0;
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
}

/* Cancel without touching the RTC. */
static void rightPressed(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    back();
}

/* Save to the RTC; stay on the screen if it is rejected. */
static void centerPressed(void)
{
    saveError = Rtc_Set(Util_GetUnixTimestamp(&editedTime, &editedDate));
    if (saveError)
    {
        Main_SetBuzzerMode(eBuzzer_Mode_LongBeep);
        return;
    }
    Main_SetBuzzerMode(eBuzzer_Mode_2ShortBeep);
    back();
}

static void noAction(void)
{
}

/* Private helpers ========================================================== */

static void windowCallback(UG_MESSAGE *message)
{
    (void)message;
}

static uint8_t monthLength(uint8_t year, uint8_t month)
{
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    return days[month - 1] + (month == 2 && !(year & 3));
}

static void createText(uint8_t id, int x, int y, int xe, int ye, char *value, UG_COLOR color)
{
    UG_TextboxCreate(&window_1, boxes[id], id, x, y, xe, ye);
    UG_TextboxSetFont(&window_1, id, DEFAULT_FONT);
    UG_TextboxSetText(&window_1, id, value);
    UG_TextboxSetBackColor(&window_1, id, color);
    UG_TextboxSetForeColor(&window_1, id, C_WHITE);
    UG_TextboxSetAlignment(&window_1, id, ALIGN_CENTER);
    UG_TextboxSetScrollEnabled(&window_1, id, 0);
}

/* Step the selected field by one, wrapping inside its range. */
static void change(int8_t direction)
{
    uint8_t *field;
    uint8_t lower = 0;
    uint8_t upper;
    uint8_t limit;

    if (selectedField == 0)
    {
        field = &editedDate.Date;
        lower = 1;
        upper = monthLength(editedDate.Year, editedDate.Month);
    }
    else if (selectedField == 1)
    {
        field = &editedDate.Month;
        lower = 1;
        upper = 12;
    }
    else if (selectedField == 2)
    {
        field = &editedDate.Year;
        lower = 24;
        upper = 99;
    }
    else if (selectedField == 3)
    {
        field = &editedTime.Hours;
        upper = 23;
    }
    else if (selectedField == 4)
    {
        field = &editedTime.Minutes;
        upper = 59;
    }
    else
    {
        field = &editedTime.Seconds;
        upper = 59;
    }

    if (direction > 0)
    {
        *field = *field >= upper ? lower : *field + 1;
    }
    else
    {
        *field = *field <= lower ? upper : *field - 1;
    }

    /* Clamp the day after a month or year change. */
    limit = monthLength(editedDate.Year, editedDate.Month);
    if (editedDate.Date > limit)
    {
        editedDate.Date = limit;
    }
    saveError = 0;
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
}

static void back(void)
{
    ScrnCommon_Switch(&generalSettingScreen);
}
