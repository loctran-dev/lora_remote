#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"

/* Message line y position: alone in the middle, or above the device info rows. */
#define MESSAGE_Y_CENTER 72
#define MESSAGE_Y_TOP    52

/* Device info rows: label on the left, value on the right. */
#define INFO_Y        80
#define INFO_HEIGHT   10
#define INFO_GAP      3
#define INFO_LABEL_X  3
#define INFO_LABEL_XE 55
#define INFO_VALUE_X  57
#define INFO_VALUE_XE 124

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

uint8_t pairingUI_Step;

static void init(void);
static void render(void);
static void leftClicked(void);
static void rightClicked(void);
static void centerClicked(void);
static void rightPressed(void);
static void noAction(void);
static void windowCallback(UG_MESSAGE *message);
static void createText(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye, char *text,
    UG_COLOR background, uint8_t alignment);
static void createButtons(char *left, char *center, char *right);
static void createMessage(char *text, int y);
static void createInfoRow(uint8_t row, char *label, char *value);
static void createDeviceInfo(uint8_t withLoraId);
static void restart(uint8_t step);
static void openMainMenu(void);

Screen_t devicePairingScreen = {
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

/* Screen handlers ========================================================== */

static void init(void)
{
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
    createText(&textbox_6, TXB_ID_5, 0, 20, 127, 37, "Device pairing", C_DARK_SLATE_BLUE, ALIGN_CENTER_LEFT);

    /* Each pairing state creates its own footer once. */
    if (pairingUI_Step == PAIRING_STATE_SEARCHING)
    {
        createButtons("", "", "Cancel ");
        createMessage("Scanning...", MESSAGE_Y_CENTER);
    }
    else if (pairingUI_Step == PAIRING_STATE_FOUND)
    {
        createButtons("Scan", "Conn", "Exit ");
        createMessage("Device found!", MESSAGE_Y_TOP);
        createDeviceInfo(0);
    }
    else if (pairingUI_Step == PAIRING_STATE_NOT_FOUND)
    {
        createButtons("Scan", "OK", "Exit ");
        createMessage("No device found", MESSAGE_Y_CENTER);
    }
    else if (pairingUI_Step == PAIRING_STATE_CONNECTING)
    {
        createButtons("", "", "Cancel ");
        createMessage("Connecting...", MESSAGE_Y_TOP);
        createDeviceInfo(0);
    }
    else if (pairingUI_Step == PAIRING_STATE_SUCCESS)
    {
        createButtons("", "Scan", "Exit ");
        createMessage("Connected!", MESSAGE_Y_TOP);
        createDeviceInfo(1);
    }
    else if (pairingUI_Step == PAIRING_STATE_FAILED)
    {
        createButtons("Retry", "Scan", "Exit ");
        createMessage("Connect failed", MESSAGE_Y_TOP);
        createDeviceInfo(0);
    }

    UG_WindowShow(&window_1);
    UG_Update();
}

static void render(void)
{
    ScrnCommon_StatusBarUpdate();
    UG_Update();
}

/* "Scan" after a search result, "Retry" after a failed connection. */
static void leftClicked(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (pairingUI_Step == PAIRING_STATE_FOUND || pairingUI_Step == PAIRING_STATE_NOT_FOUND)
    {
        restart(PAIRING_STATE_SEARCHING);
        Main_ScanNewDevice();
    }
    else if (pairingUI_Step == PAIRING_STATE_FAILED)
    {
        restart(PAIRING_STATE_CONNECTING);
        Main_ConnectToDevice();
    }
}

/* "Cancel"/"Exit" in every state. */
static void rightClicked(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (pairingUI_Step == PAIRING_STATE_SEARCHING ||
        pairingUI_Step == PAIRING_STATE_FOUND ||
        pairingUI_Step == PAIRING_STATE_NOT_FOUND ||
        pairingUI_Step == PAIRING_STATE_CONNECTING ||
        pairingUI_Step == PAIRING_STATE_SUCCESS ||
        pairingUI_Step == PAIRING_STATE_FAILED)
    {
        openMainMenu();
    }
}

/* "Conn" when found, "Scan" after a successful or failed connection. */
static void centerClicked(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    if (pairingUI_Step == PAIRING_STATE_FOUND || pairingUI_Step == PAIRING_STATE_FAILED)
    {
        restart(PAIRING_STATE_CONNECTING);
        Main_ConnectToDevice();
    }
    else if (pairingUI_Step == PAIRING_STATE_SUCCESS)
    {
        restart(PAIRING_STATE_SEARCHING);
        Main_ScanNewDevice();
    }
}

static void rightPressed(void)
{
    Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
    openMainMenu();
}

static void noAction(void)
{
}

/* Private helpers ========================================================== */

static void windowCallback(UG_MESSAGE *message)
{
    (void)message;
}

static void createText(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye, char *text,
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
    createText(&textbox_3, TXB_ID_2, 0, 144, 43, 159, left, C_RED, ALIGN_CENTER_LEFT);
    createText(&textbox_4, TXB_ID_3, 44, 144, 83, 159, center, C_RED, ALIGN_CENTER);
    createText(&textbox_5, TXB_ID_4, 84, 144, 127, 159, right, C_RED, ALIGN_CENTER_RIGHT);
}

static void createMessage(char *text, int y)
{
    createText(&textbox_7, TXB_ID_6, 15, y, 123, y + 10, text, C_BLACK, ALIGN_CENTER);
}

static void createInfoRow(uint8_t row, char *label, char *value)
{
    UG_TEXTBOX *labelBoxes[] = {&textbox_8, &textbox_10, &textbox_12, &textbox_14};
    UG_TEXTBOX *valueBoxes[] = {&textbox_9, &textbox_11, &textbox_13, &textbox_15};
    uint8_t labelIds[] = {TXB_ID_7, TXB_ID_9, TXB_ID_11, TXB_ID_13};
    uint8_t valueIds[] = {TXB_ID_8, TXB_ID_10, TXB_ID_12, TXB_ID_14};
    int y = INFO_Y + row * (INFO_HEIGHT + INFO_GAP);

    createText(labelBoxes[row], labelIds[row], INFO_LABEL_X, y, INFO_LABEL_XE, y + INFO_HEIGHT,
        label, C_BLACK, ALIGN_CENTER_LEFT);
    createText(valueBoxes[row], valueIds[row], INFO_VALUE_X, y, INFO_VALUE_XE, y + INFO_HEIGHT,
        value, C_BLACK, ALIGN_CENTER_RIGHT);
}

/* Serial, type and module count of the found device; the assigned LoRa ID after success. */
static void createDeviceInfo(uint8_t withLoraId)
{
    char snText[20];
    char typeText[20];
    char moduleCountText[20];
    char loraIdText[20];

    snprintf(snText, sizeof(snText), "0x%08lX", (unsigned long)systemInfo.findDevicefactoryInfo.serial);
    sprintf(typeText, "%c", systemInfo.findDevicefactoryInfo.deviceTypeChar);
    sprintf(moduleCountText, "%02d", systemInfo.findDevicefactoryInfo.numberOfModule);
    createInfoRow(0, "SN", snText);
    createInfoRow(1, "Type", typeText);
    createInfoRow(2, "Modules", moduleCountText);
    if (withLoraId)
    {
        sprintf(loraIdText, "0x%02X", systemInfo.nextNodeId);
        createInfoRow(3, "Lora ID", loraIdText);
    }
}

/* Re-create this screen in another pairing state. */
static void restart(uint8_t step)
{
    pairingUI_Step = step;
    ScrnCommon_Switch(&devicePairingScreen);
}

static void openMainMenu(void)
{
    ScrnCommon_Switch(&mainMenuScreen);
}
