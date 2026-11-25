#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"

/* Menu items, in display order. */
#define ITEM_DEVICE_MANAGER 0
#define ITEM_DEVICE_CONTROL 1
#define ITEM_GENERAL        2
#define ITEM_FIND_DEVICE    3
#define ITEM_HISTORY        4
#define ITEM_COUNT          5

#define ITEM_X      15
#define ITEM_XE     120
#define ITEM_Y      42
#define ITEM_HEIGHT 10
#define ITEM_GAP    3

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

static uint8_t menuIndex;
static const uint8_t itemIds[ITEM_COUNT] = {TXB_ID_6, TXB_ID_8, TXB_ID_10, TXB_ID_12, TXB_ID_14};

static void init(void);
static void render(void);
static void leftClicked(void);
static void rightClicked(void);
static void centerClicked(void);
static void rightPressed(void);
static void noAction(void);
static void windowCallback(UG_MESSAGE *message);
static void createButton(UG_TEXTBOX *box, uint8_t id, int x, int xe, char *text, uint8_t alignment);
static void createMenuItem(UG_TEXTBOX *box, uint8_t item, char *text);
static void highlightSelection(void);

Screen_t mainMenuScreen = {
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

static void init(void)
{
    /* Window and status bar. */
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

    /* Button labels. */
    createButton(&textbox_3, TXB_ID_2, 0, 43, " Up", ALIGN_CENTER_LEFT);
    createButton(&textbox_4, TXB_ID_3, 44, 83, "Go to", ALIGN_CENTER);
    createButton(&textbox_5, TXB_ID_4, 84, 127, "Down ", ALIGN_CENTER_RIGHT);

    /* Title. */
    UG_TextboxCreate(&window_1, &textbox_6, TXB_ID_5, 0, 20, 127, 37);
    UG_TextboxSetFont(&window_1, TXB_ID_5, DEFAULT_FONT);
    UG_TextboxSetText(&window_1, TXB_ID_5, "Menu");
    UG_TextboxSetBackColor(&window_1, TXB_ID_5, C_DARK_SLATE_BLUE);
    UG_TextboxSetForeColor(&window_1, TXB_ID_5, C_WHITE);
    UG_TextboxSetAlignment(&window_1, TXB_ID_5, ALIGN_CENTER_LEFT);

    /* Menu items; the first one starts selected. */
    createMenuItem(&textbox_7, ITEM_DEVICE_MANAGER, "Device manager");
    createMenuItem(&textbox_9, ITEM_DEVICE_CONTROL, "Device control");
    createMenuItem(&textbox_11, ITEM_GENERAL, "General setting");
    createMenuItem(&textbox_13, ITEM_FIND_DEVICE, "Find new device");
    createMenuItem(&textbox_15, ITEM_HISTORY, "History");
    UG_TextboxSetBackColor(&window_1, itemIds[0], C_DIM_GRAY);
    menuIndex = 0;

    ScrnCommon_MenuScrollUpdate(ITEM_COUNT, menuIndex);
    UG_WindowShow(&window_1);
    UG_Update();
}

static void render(void)
{
    ScrnCommon_MenuScrollUpdate(ITEM_COUNT, menuIndex);
    ScrnCommon_StatusBarUpdate();
    UG_Update();
}

/* Move the selection up. */
static void leftClicked(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    if (menuIndex > 0)
    {
        menuIndex--;
        highlightSelection();
    }
}

/* Move the selection down. */
static void rightClicked(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    if (menuIndex < ITEM_COUNT - 1)
    {
        menuIndex++;
        highlightSelection();
    }
}

/* Open the selected item. */
static void centerClicked(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    if (menuIndex == ITEM_HISTORY)
    {
        ScrnHs_OpenHistory();
    }
    else if (menuIndex == ITEM_DEVICE_MANAGER)
    {
        ScrnCommon_Switch(&deviceManagerScreen);
    }
    else if (menuIndex == ITEM_DEVICE_CONTROL)
    {
        ScrnCommon_Switch(&deviceControlScreen);
    }
    else if (menuIndex == ITEM_GENERAL)
    {
        ScrnGss_ResetSelection();
        ScrnCommon_Switch(&generalSettingScreen);
    }
    else if (menuIndex == ITEM_FIND_DEVICE)
    {
        pairingUI_Step = PAIRING_STATE_SEARCHING;
        ScrnCommon_Switch(&devicePairingScreen);
        Main_ScanNewDevice();
    }
}

static void rightPressed(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
}

static void noAction(void)
{
}

static void windowCallback(UG_MESSAGE *message)
{
    (void)message;
}

static void createButton(UG_TEXTBOX *box, uint8_t id, int x, int xe, char *text, uint8_t alignment)
{
    UG_TextboxCreate(&window_1, box, id, x, 144, xe, 159);
    UG_TextboxSetFont(&window_1, id, DEFAULT_FONT);
    UG_TextboxSetText(&window_1, id, text);
    UG_TextboxSetBackColor(&window_1, id, C_RED);
    UG_TextboxSetForeColor(&window_1, id, C_WHITE);
    UG_TextboxSetAlignment(&window_1, id, alignment);
}

static void createMenuItem(UG_TEXTBOX *box, uint8_t item, char *text)
{
    uint16_t y = ITEM_Y + item * (ITEM_HEIGHT + ITEM_GAP);

    UG_TextboxCreate(&window_1, box, itemIds[item], ITEM_X, y, ITEM_XE, y + ITEM_HEIGHT);
    UG_TextboxSetFont(&window_1, itemIds[item], DEFAULT_FONT);
    UG_TextboxSetText(&window_1, itemIds[item], text);
    UG_TextboxSetBackColor(&window_1, itemIds[item], C_BLACK);
    UG_TextboxSetForeColor(&window_1, itemIds[item], C_WHITE);
    UG_TextboxSetAlignment(&window_1, itemIds[item], ALIGN_CENTER_LEFT);
}

/* Grey the selected item and clear its neighbours. */
static void highlightSelection(void)
{
    UG_TextboxSetBackColor(&window_1, itemIds[menuIndex], C_DIM_GRAY);
    if (itemIds[menuIndex] < TXB_ID_14)
    {
        UG_TextboxSetBackColor(&window_1, itemIds[menuIndex] + 2, C_BLACK);
    }
    if (itemIds[menuIndex] > TXB_ID_6)
    {
        UG_TextboxSetBackColor(&window_1, itemIds[menuIndex] - 2, C_BLACK);
    }
    ScrnCommon_MenuScrollUpdate(ITEM_COUNT, menuIndex);
    UG_Update();
}
