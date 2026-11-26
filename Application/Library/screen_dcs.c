#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "image_data.h"
#include "screens.h"
#include "main.h"
#include "config_store.h"
#include "utility.h"
#include "power_sleep.h"
#include "schedule.h"

#define TIMER_PRESET_MAX     100  /* Preset 0 = custom, 1 = disable, n = (n - 1) x 30 min. */
#define TIMER_ROWS           4
#define TIMER_DIGITS         4
#define DIGIT_BACKGROUND     0x2104
#define ICON_BACKGROUND      0x0841
#define LOCK_NOTICE_MS       3000u
#define CONNECTION_NOTICE_MS 6000u

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

/* Timer selection state. */
static uint8_t inScheduleSelection = 0;
static uint8_t indexScheduleSelection = 0;
static uint8_t indexScheduleValue = 0;
static uint8_t adjustingScheduleValue = 0;
/* Timer UI uses numeric state, not cursor positions in a formatted string. */
static uint16_t scheduleCustomMinutes;
static uint8_t timerZeroError;

/* Status icon, drawn by a custom uGUI object only when its inputs change. */
static UG_OBJECT *statusIconsObject;
static uint32_t statusIconsKey;
static DeviceInfo_t statusIconDevice;
static MotorData_t statusIconModule;

/* Temporary notices in the title bar. */
static volatile uint8_t lockNoticeActive;
static volatile uint8_t lockNoticePower;
static volatile uint32_t lockNoticeTick;
static int8_t connectionNotice = -1;
static uint32_t connectionNoticeTick;

static void init(void);
static void render(void);
static void leftClicked(void);
static void rightClicked(void);
static void centerClicked(void);
static void leftPressed(void);
static void rightPressed(void);
static void centerPressed(void);
static void bLeftPressed(void);
static void bRightPressed(void);
static void noAction(void);
static void windowCallback(UG_MESSAGE *message);
static void createLabel(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye, char *text,
    UG_COLOR background, UG_COLOR foreground, uint8_t alignment);
static void createText(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye,
    char *text, UG_FONT *font, UG_COLOR background);
static void drawIconText(char *text, int16_t xs, int16_t ys, int16_t xe, int16_t ye,
    UG_COLOR color, UG_COLOR background);
static void drawStatusIcons(const DeviceInfo_t *device, const MotorData_t *module);
static void statusIconsUpdate(UG_WINDOW *window, UG_OBJECT *object);
static void setStatusIcons(const DeviceInfo_t *device, const MotorData_t *module);
static void splitMinutes(uint8_t digits[TIMER_DIGITS]);
static void timerRender(void);
static void createTimerScreen(void);
static void adjustScheduleDigit(uint8_t increase);
static void returnFromTimer(void);
static void formatRemaining(uint32_t seconds, char *text);
static void countdownUpdate(const DeviceInfo_t *device, const MotorData_t *module);

/* LEFT/RIGHT: previous/next motor, M11 M12 .. M21 .. (timer: up/down or +/-), CENTER: timer.
   Hold LEFT: monitor, RIGHT: settings, CENTER: menu; hold bottom buttons: motor ON/OFF. */
Screen_t deviceControlScreen = {
    0,
    init,
    render,
    leftClicked,
    rightClicked,
    centerClicked,
    noAction,
    noAction,
    leftPressed,
    rightPressed,
    centerPressed,
    bLeftPressed,
    bRightPressed
};

/**
 * @brief  Report a lost connection: wake the display, beep three times and show the device.
 * @param  id  Device index in deviceBuff.
 * @note   Switches to device control unless an editor, the history or a
 *         schedule selection is open; the notice is shown on that screen.
 */
void ScrnDcs_ShowConnectionError(uint8_t id)
{
    Power_WakeForError();
    Main_SetBuzzerMode(eBuzzer_Mode_3ShortBeep);
    if (ScrnCommon_DeviceVisible(id) && currentScreen != &editDeviceSettingScreen &&
        currentScreen != &editGeneralSettingScreen && currentScreen != &dateTimeScreen &&
        currentScreen != &historyScreen && !inScheduleSelection)
    {
        UG_WindowHide(&window_1);
        UG_WindowDelete(&window_1);
        systemInfo.curDevice = id;
        systemInfo.curModule = 0;
        ScrnCommon_EnsureVisibleDevice(); /* first visible motor of the device */
        currentScreen = &deviceControlScreen;
        currentScreen->init();
    }
    connectionNotice = id;
    connectionNoticeTick = HAL_GetTick();
}

/**
 * @brief  Show the "motor locked" notice on device control.
 * @param  motorPower  Power state that was refused, a Motor_PowerState_e value.
 */
void ScrnDcs_ShowMotorLocked(uint8_t motorPower)
{
    lockNoticePower = motorPower;
    lockNoticeTick = HAL_GetTick();
    lockNoticeActive = 1;
}

/**
 * @brief  Whether the schedule/timer selection is open.
 * @return 1 while open; the remote does not sleep then.
 */
uint8_t ScrnDcs_IsScheduleEditing(void)
{
    return inScheduleSelection != 0;
}

static void init(void)
{
    DeviceInfo_t device;
    MotorData_t module;
    char title[30];
    char currentAText[20];
    char currentBText[20];
    char currentCText[20];
    char rssiText[20];

    lockNoticeActive = 0;
    if (!ScrnCommon_EnsureVisibleDevice())
    {
        inScheduleSelection = 0;
        ScrnCommon_EmptyControlInit();
        return;
    }
    if (inScheduleSelection)
    {
        createTimerScreen();
        return;
    }

    /* Window with the custom status icon object. */
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
    statusIconsObject = _UG_GetFreeObject(&window_1);
    statusIconsKey = 0xFFFFFFFFu;
    if (statusIconsObject)
    {
        statusIconsObject->type = 8;
        statusIconsObject->id = 240;
        statusIconsObject->update = statusIconsUpdate;
        statusIconsObject->state = OBJ_STATE_VALID | OBJ_STATE_VISIBLE;
    }
    ScrnCommon_StatusBarCreate();
    /* Power status is drawn with crisp LCD primitives after widget updates. */

    /* Button labels. */
    createLabel(&textbox_3, TXB_ID_2, 0, 144, 35, 159, " Pre", C_RED, C_WHITE, ALIGN_CENTER_LEFT);
    createLabel(&textbox_4, TXB_ID_3, 36, 144, 91, 159, "Set time", C_RED, C_WHITE, ALIGN_CENTER);
    createLabel(&textbox_5, TXB_ID_4, 92, 144, 127, 159, "Next ", C_RED, C_WHITE, ALIGN_CENTER_RIGHT);

    /* Title, RSSI and the three phase currents. */
    device = deviceBuff[systemInfo.curDevice];
    module = *ScrnCommon_CurrentMotorState();
    snprintf(title, sizeof(title), " %c%u%u.%.19s", storageData.DeviceConfig[device.index].factoryInfo.deviceTypeChar,
        device.index + 1, systemInfo.curModule + 1,
        ScrnCommon_MotorName(device.index, systemInfo.curModule));
    sprintf(currentAText, "%5.2f A", module.phaseCurrent[0]);
    sprintf(currentBText, "%5.2f A", module.phaseCurrent[1]);
    sprintf(currentCText, "%5.2f A", module.phaseCurrent[2]);
    sprintf(rssiText, "%ddbm  ", device.rssi);
    createLabel(&textbox_10, TXB_ID_9, 0, 20, 80, 37, title, C_DARK_SLATE_BLUE, C_WHITE, ALIGN_CENTER_LEFT);
    createLabel(&textbox_11, TXB_ID_10, 80, 20, 127, 37, rssiText, C_DARK_SLATE_BLUE, C_GREEN, ALIGN_CENTER_RIGHT);
    createLabel(&textbox_6, TXB_ID_5, 80, 47, 123, 57, currentAText, C_BLACK, C_WHITE, ALIGN_CENTER_RIGHT);
    createLabel(&textbox_7, TXB_ID_6, 80, 61, 123, 71, currentBText, C_BLACK, C_WHITE, ALIGN_CENTER_RIGHT);
    createLabel(&textbox_8, TXB_ID_7, 80, 75, 123, 85, currentCText, C_BLACK, C_WHITE, ALIGN_CENTER_RIGHT);

    /* Timer summary and countdown. */
    createText(&textbox_12, 11, 4, 94, 65, 104, "Auto stop", DEFAULT_FONT, C_BLACK);
    UG_TextboxSetAlignment(&window_1, 11, ALIGN_CENTER_LEFT);
    createText(&textbox_13, 12, 66, 94, 123, 104, "Not set", DEFAULT_FONT, C_BLACK);
    UG_TextboxSetAlignment(&window_1, 12, ALIGN_CENTER_RIGHT);
    createText(&textbox_9, TXB_ID_8, 4, 108, 123, 132, "", FONT_12X20, C_BLACK);
    UG_TextboxSetForeColor(&window_1, TXB_ID_8, C_CYAN);

    UG_WindowShow(&window_1);
    UG_Update();
}

static void render(void)
{
    DeviceInfo_t device;
    MotorData_t module;
    char title[30];
    char currentAText[20];
    char currentBText[20];
    char currentCText[20];
    char rssiText[20];
    char lost[16];
    UG_COLOR rssiColor;

    if (!ScrnCommon_SelectionVisible())
    {
        ScrnCommon_EmptyControlRender();
        return;
    }
    if (inScheduleSelection)
    {
        timerRender();
        return;
    }

    /* Title (or lock notice) and phase currents. */
    device = deviceBuff[systemInfo.curDevice];
    module = *ScrnCommon_CurrentMotorState();
    snprintf(title, sizeof(title), " %c%u%u.%.19s", storageData.DeviceConfig[device.index].factoryInfo.deviceTypeChar,
        device.index + 1, systemInfo.curModule + 1,
        ScrnCommon_MotorName(device.index, systemInfo.curModule));
    sprintf(currentAText, "%5.2f A", module.phaseCurrent[0]);
    sprintf(currentBText, "%5.2f A", module.phaseCurrent[1]);
    sprintf(currentCText, "%5.2f A", module.phaseCurrent[2]);
    if (lockNoticeActive && (!ScrnCommon_CurrentMotor()->locked || (uint32_t)(HAL_GetTick() - lockNoticeTick) >= LOCK_NOTICE_MS))
    {
        lockNoticeActive = 0;
    }
    UG_TextboxSetBackColor(&window_1, TXB_ID_9, lockNoticeActive ? C_RED : C_DARK_SLATE_BLUE);
    UG_TextboxSetText(&window_1, TXB_ID_9, lockNoticeActive ?
        (lockNoticePower == eMotor_PowerState_On ? "ON BLOCKED" : "OFF BLOCKED") : title);
    UG_TextboxSetText(&window_1, TXB_ID_5, currentAText);
    UG_TextboxSetText(&window_1, TXB_ID_6, currentBText);
    UG_TextboxSetText(&window_1, TXB_ID_7, currentCText);
    countdownUpdate(&device, &module);

    /* RSSI, coloured by signal level. */
    if (device.linkLost == TRUE)
    {
        sprintf(rssiText, "!Error  ");
        rssiColor = C_RED;
    }
    else
    {
        sprintf(rssiText, "%ddbm  ", device.rssi);
        rssiColor = device.rssi > RSSI_LEVEL_1 ? C_TOMATO :
            device.rssi > RSSI_LEVEL_2 ? C_ORANGE :
            device.rssi > RSSI_LEVEL_3 ? C_YELLOW :
            device.rssi > RSSI_LEVEL_4 ? C_GREEN_YELLOW :
            device.rssi > RSSI_LEVEL_5 ? C_LIGHT_GREEN : C_GREEN;
    }
    UG_TextboxSetForeColor(&window_1, TXB_ID_10, rssiColor);
    UG_TextboxSetText(&window_1, TXB_ID_10, rssiText);

    /* Apply after the RSSI update so the notification remains visible. */
    if (connectionNotice >= 0)
    {
        if (!deviceBuff[connectionNotice].linkLost ||
            (uint32_t)(HAL_GetTick() - connectionNoticeTick) >= CONNECTION_NOTICE_MS)
        {
            connectionNotice = -1;
        }
        else
        {
            snprintf(lost, sizeof(lost), "M%u1", connectionNotice + 1);
            UG_TextboxSetBackColor(&window_1, TXB_ID_9, C_RED);
            UG_TextboxSetText(&window_1, TXB_ID_9, "NO STATUS");
            UG_TextboxSetText(&window_1, TXB_ID_10, lost);
        }
    }
    UG_TextboxSetBackColor(&window_1, TXB_ID_10,
        (lockNoticeActive || connectionNotice >= 0) ? C_RED : C_DARK_SLATE_BLUE);
    if (lockNoticeActive)
    {
        UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_WHITE);
        UG_TextboxSetText(&window_1, TXB_ID_10, "LOCKED");
    }
    if (connectionNotice >= 0)
    {
        snprintf(lost, sizeof(lost), "M%u1", connectionNotice + 1);
        UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_WHITE);
        UG_TextboxSetText(&window_1, TXB_ID_10, lost);
    }

    ScrnCommon_StatusBarUpdate();
    setStatusIcons(&device, &module);
    UG_Update();
}

static void leftClicked(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    if (!ScrnCommon_SelectionVisible())
    {
        return;
    }
    if (!inScheduleSelection)
    {
        ScrnCommon_SelectAdjacentVisible(-1);
    }
    else if (adjustingScheduleValue)
    {
        adjustScheduleDigit(1);
    }
    else if (indexScheduleSelection)
    {
        indexScheduleSelection--;
    }
    if (inScheduleSelection)
    {
        timerRender();
    }
}

static void rightClicked(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    if (!ScrnCommon_SelectionVisible())
    {
        return;
    }
    if (!inScheduleSelection)
    {
        ScrnCommon_SelectAdjacentVisible(1);
    }
    else if (adjustingScheduleValue)
    {
        adjustScheduleDigit(0);
    }
    else if (indexScheduleSelection < TIMER_PRESET_MAX)
    {
        indexScheduleSelection++;
    }
    if (inScheduleSelection)
    {
        timerRender();
    }
}

/* Open the timer; inside it select a preset, enter custom time, or move the digit cursor. */
static void centerClicked(void)
{
    int32_t seconds;

    if (!ScrnCommon_SelectionVisible())
    {
        ScrnCommon_Switch(&deviceManagerScreen);
        return;
    }
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    if (!inScheduleSelection)
    {
        inScheduleSelection = 1;
        adjustingScheduleValue = indexScheduleValue = indexScheduleSelection = timerZeroError = 0;
        scheduleCustomMinutes = 0;
        UG_WindowHide(&window_1);
        UG_WindowDelete(&window_1);
        createTimerScreen();
    }
    else if (adjustingScheduleValue)
    {
        indexScheduleValue = (indexScheduleValue + 1) % TIMER_DIGITS;
        timerRender();
    }
    else if (!indexScheduleSelection)
    {
        adjustingScheduleValue = 1;
        indexScheduleValue = timerZeroError = 0;
        UG_WindowHide(&window_1);
        UG_WindowDelete(&window_1);
        createTimerScreen();
    }
    else
    {
        seconds = indexScheduleSelection == 1 ? -1 : (indexScheduleSelection - 1) * 1800;
        Main_SetScheduleTimeToCurrentModule(seconds);
        returnFromTimer();
    }
}

/* Open the device monitor. */
static void leftPressed(void)
{
    if (inScheduleSelection || !ScrnCommon_SelectionVisible())
    {
        return;
    }
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    ScrnCommon_Switch(&deviceMonitorScreen);
}

/* Leave the timer, or open the device settings. */
static void rightPressed(void)
{
    if (inScheduleSelection)
    {
        Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
        returnFromTimer();
        return;
    }
    if (!ScrnCommon_SelectionVisible())
    {
        ScrnCommon_Switch(&deviceManagerScreen);
        return;
    }
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    ScrnDss_ResetSelection();
    ScrnCommon_Switch(&deviceSettingScreen);
}

/* Main menu; in the timer: back from presets, or apply the custom time. */
static void centerPressed(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    if (!inScheduleSelection)
    {
        ScrnCommon_Switch(&mainMenuScreen);
    }
    else if (!adjustingScheduleValue)
    {
        returnFromTimer(); /* Back does not submit the highlighted preset. */
    }
    else if (!scheduleCustomMinutes)
    {
        timerZeroError = 1;
        Main_SetBuzzerMode(eBuzzer_Mode_LongBeep);
        timerRender();
    }
    else
    {
        Main_SetScheduleTimeToCurrentModule((int32_t)scheduleCustomMinutes * 60);
        returnFromTimer();
    }
}

static void bLeftPressed(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    Main_SetMotorPower(eMotor_PowerState_On);
}

static void bRightPressed(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    Main_SetMotorPower(eMotor_PowerState_Off);
}

static void noAction(void)
{
}

static void windowCallback(UG_MESSAGE *message)
{
    (void)message;
}

static void createLabel(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye, char *text,
    UG_COLOR background, UG_COLOR foreground, uint8_t alignment)
{
    UG_TextboxCreate(&window_1, box, id, x, y, xe, ye);
    UG_TextboxSetFont(&window_1, id, DEFAULT_FONT);
    UG_TextboxSetText(&window_1, id, text);
    UG_TextboxSetBackColor(&window_1, id, background);
    UG_TextboxSetForeColor(&window_1, id, foreground);
    UG_TextboxSetAlignment(&window_1, id, alignment);
}

/* Centred, non-scrolling text in any font. */
static void createText(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye,
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

static void drawIconText(char *text, int16_t xs, int16_t ys, int16_t xe, int16_t ye,
    UG_COLOR color, UG_COLOR background)
{
    UG_TEXT label = {0};

    label.a.xs = xs;
    label.a.ys = ys;
    label.a.xe = xe;
    label.a.ye = ye;
    label.font = DEFAULT_FONT;
    label.str = text;
    label.align = ALIGN_CENTER;
    label.fc = color;
    label.bc = background;
    _UG_PutText(&label);
}

static void drawStatusIcons(const DeviceInfo_t *device, const MotorData_t *module)
{
    UG_COLOR color = C_ORANGE;
    char *status = "UNKNOWN";
    uint8_t known = !storageData.DeviceConfig[device->index].deleted && !device->linkLost &&
        (module->pwrState == eMotor_PowerState_On || module->pwrState == eMotor_PowerState_Off);

    /* Status text and colour. */
    if (storageData.DeviceConfig[device->index].deleted || device->linkLost)
    {
        status = "NO LINK";
    }
    else if (module->alarmReported)
    {
        status = "ALARM";
        color = C_RED;
    }
    else if (known)
    {
        status = module->pwrState == eMotor_PowerState_On ? "ON" : "OFF";
        color = module->pwrState == eMotor_PowerState_On ? C_GREEN : C_RED;
    }

    /* Frame, then a power symbol (known state) or a warning triangle. */
    UG_FillFrame(9, 44, 69, 88, C_BLACK);
    UG_FillRoundFrame(10, 45, 68, 87, 6, ICON_BACKGROUND);
    UG_DrawRoundFrame(10, 45, 68, 87, 6, color);
    if (known && !module->alarmReported)
    {
        UG_DrawCircle(39, 60, 10, color);
        UG_DrawCircle(39, 60, 9, color);
        UG_FillFrame(35, 48, 43, 55, ICON_BACKGROUND);
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

    /* Status text and phase labels. */
    drawIconText(status, 12, 74, 66, 84, color, ICON_BACKGROUND);
    drawIconText("A", 71, 47, 78, 57, C_CYAN, C_BLACK);
    drawIconText("B", 71, 61, 78, 71, C_CYAN, C_BLACK);
    drawIconText("C", 71, 75, 78, 85, C_CYAN, C_BLACK);
}

static void statusIconsUpdate(UG_WINDOW *window, UG_OBJECT *object)
{
    (void)window;
    if ((object->state & OBJ_STATE_UPDATE) && (object->state & OBJ_STATE_REDRAW))
    {
        drawStatusIcons(&statusIconDevice, &statusIconModule);
    }
    object->state &= ~(OBJ_STATE_UPDATE | OBJ_STATE_REDRAW);
}

/* Request a redraw only when one of the drawn inputs changed. */
static void setStatusIcons(const DeviceInfo_t *device, const MotorData_t *module)
{
    uint32_t key = (storageData.DeviceConfig[device->index].deleted ? 1u : 0u) |
        (device->linkLost ? 2u : 0u) |
        (module->alarmReported ? 4u : 0u) |
        ((uint32_t)module->pwrState << 8) |
        (module->autoOffTime > 0 ? 0x10000u : 0u);

    if (statusIconsObject && key != statusIconsKey)
    {
        statusIconsKey = key;
        statusIconDevice = *device;
        statusIconModule = *module;
        statusIconsObject->state |= OBJ_STATE_UPDATE | OBJ_STATE_REDRAW;
    }
}

/* HH:MM digits of the custom duration. */
static void splitMinutes(uint8_t digits[TIMER_DIGITS])
{
    digits[0] = scheduleCustomMinutes / 60 / 10;
    digits[1] = scheduleCustomMinutes / 60 % 10;
    digits[2] = scheduleCustomMinutes % 60 / 10;
    digits[3] = scheduleCustomMinutes % 10;
}

static void timerRender(void)
{
    char text[32];
    uint8_t digits[TIMER_DIGITS];
    char digit[2];
    uint8_t page;
    uint8_t item;
    uint8_t id;
    uint16_t minutes;

    ScrnCommon_StatusBarUpdate();
    if (adjustingScheduleValue)
    {
        /* Custom duration: four digits with the cursor highlighted. */
        splitMinutes(digits);
        for (uint8_t i = 0; i < TIMER_DIGITS; i++)
        {
            digit[0] = '0' + digits[i];
            digit[1] = 0;
            UG_TextboxSetText(&window_1, 32 + i, digit);
            UG_TextboxSetBackColor(&window_1, 32 + i, i == indexScheduleValue ? C_YELLOW : DIGIT_BACKGROUND);
            UG_TextboxSetForeColor(&window_1, 32 + i, i == indexScheduleValue ? C_BLACK : C_WHITE);
        }
        UG_TextboxSetText(&window_1, 39, timerZeroError ? "Minimum: 1 minute" : "Hold CENTER: Apply");
        UG_TextboxSetForeColor(&window_1, 39, timerZeroError ? C_ORANGE : C_WHITE);
    }
    else
    {
        /* Preset list, four per page. */
        page = indexScheduleSelection / TIMER_ROWS * TIMER_ROWS;
        snprintf(text, sizeof(text), "%u/26", page / TIMER_ROWS + 1);
        UG_TextboxSetText(&window_1, TXB_ID_10, text);
        for (uint8_t row = 0; row < TIMER_ROWS; row++)
        {
            item = page + row;
            id = 11 + row;
            if (item > TIMER_PRESET_MAX)
            {
                if (UG_TextboxCheckVisible(&window_1, id))
                {
                    UG_TextboxHide(&window_1, id);
                }
                continue;
            }
            if (!UG_TextboxCheckVisible(&window_1, id))
            {
                UG_TextboxShow(&window_1, id);
            }
            if (!item)
            {
                strcpy(text, "Custom time");
            }
            else if (item == 1)
            {
                strcpy(text, "Disable timer");
            }
            else
            {
                minutes = (item - 1) * 30;
                snprintf(text, sizeof(text), "%02u h %02u min", minutes / 60, minutes % 60);
            }
            UG_TextboxSetText(&window_1, id, text);
            UG_TextboxSetBackColor(&window_1, id, item == indexScheduleSelection ? C_DIM_GRAY : C_BLACK);
            UG_TextboxSetForeColor(&window_1, id, item == indexScheduleSelection ? C_CYAN : C_WHITE);
        }
    }
    UG_Update();
}

static void createTimerScreen(void)
{
    UG_TEXTBOX *boxes[] = {&textbox_12, &textbox_13, &textbox_14, &textbox_15};
    char title[24];
    DeviceInfo_t *device;
    char type;
    int x;

    /* Window, status bar and title. */
    UG_WindowCreate(&window_1, obj_buff_wnd_1, MAX_OBJECTS, windowCallback);
    UG_WindowSetStyle(&window_1, WND_STYLE_HIDE_TITLE);
    UG_WindowSetTitleHeight(&window_1, 0);
    UG_WindowSetBackColor(&window_1, C_BLACK);
    UG_WindowSetXStart(&window_1, 0);
    UG_WindowSetYStart(&window_1, 0);
    UG_WindowSetXEnd(&window_1, 127);
    UG_WindowSetYEnd(&window_1, 159);
    ScrnCommon_StatusBarCreate();
    device = &deviceBuff[systemInfo.curDevice];
    type = storageData.DeviceConfig[device->index].factoryInfo.deviceTypeChar;
    if (type < 32 || type > 126)
    {
        type = 'M';
    }
    snprintf(title, sizeof(title), "%c%u%u Timer", type, device->index + 1, systemInfo.curModule + 1);
    createText(&textbox_10, 9, 0, 20, 80, 37, title, DEFAULT_FONT, C_DARK_SLATE_BLUE);
    createText(&textbox_11, 10, 81, 20, 127, 37, adjustingScheduleValue ? "HH:MM" : "", DEFAULT_FONT,
        C_DARK_SLATE_BLUE);

    /* Button labels. */
    createText(&textbox_3, 2, 0, 144, 35, 159, adjustingScheduleValue ? "+" : "Up", DEFAULT_FONT, C_DARK_SLATE_BLUE);
    createText(&textbox_4, 3, 36, 144, 91, 159, adjustingScheduleValue ? "Next" : "Select", DEFAULT_FONT,
        C_DARK_CYAN);
    createText(&textbox_5, 4, 92, 144, 127, 159, adjustingScheduleValue ? "-" : "Down", DEFAULT_FONT,
        C_DARK_SLATE_BLUE);

    if (adjustingScheduleValue)
    {
        /* Custom duration editor: HH:MM digits. */
        createText(&textbox_6, 11, 0, 43, 127, 55, "Custom duration", DEFAULT_FONT, C_BLACK);
        for (uint8_t i = 0; i < TIMER_DIGITS; i++)
        {
            x = 14 + i * 22 + (i >= 2 ? 14 : 0);
            createText(boxes[i], 32 + i, x, 65, x + 19, 94, "0", FONT_12X20, DIGIT_BACKGROUND);
        }
        createText(&textbox_16, 36, 58, 65, 69, 94, ":", FONT_12X20, C_BLACK);
        createText(&textbox_17, 37, 14, 98, 55, 109, "Hours", DEFAULT_FONT, C_BLACK);
        createText(&textbox_18, 38, 72, 98, 113, 109, "Minutes", DEFAULT_FONT, C_BLACK);
        createText(&textbox_19, 39, 0, 115, 127, 125, "", DEFAULT_FONT, C_BLACK);
        createText(&textbox_20, 40, 0, 130, 127, 140, "Hold RIGHT: Cancel", DEFAULT_FONT, C_BLACK);
    }
    else
    {
        /* Preset list rows. */
        for (uint8_t row = 0; row < TIMER_ROWS; row++)
        {
            createText(boxes[row], 11 + row, 4, 43 + row * 22, 123, 62 + row * 22, "", DEFAULT_FONT, C_BLACK);
        }
        createText(&textbox_19, 39, 0, 132, 127, 141, "Hold RIGHT: Back", DEFAULT_FONT, C_BLACK);
    }
    UG_WindowShow(&window_1);
    timerRender();
}

/* Step the digit under the cursor; the tens of minutes wrap at 6. */
static void adjustScheduleDigit(uint8_t increase)
{
    uint8_t digits[TIMER_DIGITS];
    uint8_t radix;

    splitMinutes(digits);
    if (indexScheduleValue > 3)
    {
        indexScheduleValue = 0;
    }
    radix = indexScheduleValue == 2 ? 6 : 10;
    digits[indexScheduleValue] = (digits[indexScheduleValue] + (increase ? 1 : radix - 1)) % radix;
    scheduleCustomMinutes = (digits[0] * 10 + digits[1]) * 60 + digits[2] * 10 + digits[3];
    timerZeroError = 0;
}

static void returnFromTimer(void)
{
    inScheduleSelection = adjustingScheduleValue = 0;
    timerZeroError = 0;
    ScrnCommon_Switch(&deviceControlScreen);
}

static void formatRemaining(uint32_t seconds, char *text)
{
    if (seconds > 359999u)
    {
        seconds = 359999u; /* Maximum 99:59:59, no overflow. */
    }
    snprintf(text, 9, "%02lu:%02lu:%02lu", (unsigned long)(seconds / 3600),
        (unsigned long)(seconds / 60 % 60), (unsigned long)(seconds % 60));
}

/* Timer summary: pending/failed command, "Not set", or deadline and countdown. */
static void countdownUpdate(const DeviceInfo_t *device, const MotorData_t *module)
{
    Schedule_State_e state = Schedule_GetStateForNode(storageData.DeviceConfig[device->index].nodeId);
    uint8_t present;
    char text[16];
    uint32_t remaining;
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;

    if (state == eSchedule_State_Waiting || state >= eSchedule_State_Rejected)
    {
        UG_TextboxSetText(&window_1, 11, "Timer");
        UG_TextboxSetText(&window_1, 12, state == eSchedule_State_Waiting ? "Sending" :
            state == eSchedule_State_Rejected ? "Rejected" :
            state == eSchedule_State_Timeout ? "No reply" : "Queue err");
        UG_TextboxSetForeColor(&window_1, 12, state == eSchedule_State_Waiting ? C_CYAN : C_ORANGE);
        if (UG_TextboxCheckVisible(&window_1, TXB_ID_8))
        {
            UG_TextboxHide(&window_1, TXB_ID_8);
        }
        return;
    }

    UG_TextboxSetForeColor(&window_1, 12, C_WHITE);
    present = module->pwrState == eMotor_PowerState_On && module->autoOffTime != 0;
    UG_TextboxSetText(&window_1, 11, device->linkLost && present ? "Last timer" : "Auto stop");
    if (!present)
    {
        UG_TextboxSetText(&window_1, 12, "Not set");
        if (UG_TextboxCheckVisible(&window_1, TXB_ID_8))
        {
            UG_TextboxHide(&window_1, TXB_ID_8);
        }
        return;
    }

    remaining = module->autoOffTime > systemInfo.currentTime ? module->autoOffTime - systemInfo.currentTime : 0;
    Util_ConvertTimestampToRTC(module->autoOffTime, &time, &date);
    snprintf(text, sizeof(text), "At %02u:%02u", time.Hours, time.Minutes);
    UG_TextboxSetText(&window_1, 12, device->linkLost ? "Offline" : text);
    formatRemaining(remaining, text);
    UG_TextboxSetText(&window_1, TXB_ID_8, text);
    UG_TextboxSetForeColor(&window_1, TXB_ID_8, device->linkLost || !remaining ? C_ORANGE : C_CYAN);
    if (!UG_TextboxCheckVisible(&window_1, TXB_ID_8))
    {
        UG_TextboxShow(&window_1, TXB_ID_8);
    }
}
