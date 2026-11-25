#include "link_monitor.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"
#include "config_store.h"
#include "cmsis_os.h"

#define INFO_ROWS 7

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

static void init(void);
static void render(void);
static void leftClicked(void);
static void rightClicked(void);
static void leftPressed(void);
static void noAction(void);
static void windowCallback(UG_MESSAGE *message);
static void createText(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye, char *text);

/* Read-only device information. LEFT opens this device's history; RIGHT (click or hold) goes back. */
Screen_t deviceMonitorScreen = {
    0,
    init,
    render,
    leftClicked,
    rightClicked,
    noAction,
    noAction,
    noAction,
    leftPressed,
    rightClicked,
    noAction,
    noAction,
    noAction
};

static void init(void)
{
    UG_TEXTBOX *labels[] = {&textbox_7, &textbox_9, &textbox_11, &textbox_13, &textbox_15, &textbox_17, &textbox_19};
    UG_TEXTBOX *values[] = {&textbox_8, &textbox_10, &textbox_12, &textbox_14, &textbox_16, &textbox_18, &textbox_20};
    char *names[] = {"Serial", "Node", "Type", "Modules", "Module", "RSSI", "Last RX"};
    int y;

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
    createText(&textbox_6, 5, 0, 20, 127, 37, "Device info");
    UG_TextboxSetBackColor(&window_1, 5, C_DARK_SLATE_BLUE);
    UG_TextboxSetScrollEnabled(&window_1, 5, 1);

    /* Label/value rows. */
    for (uint8_t row = 0; row < INFO_ROWS; row++)
    {
        y = 42 + row * 14;
        createText(labels[row], 6 + row * 2, 3, y, 55, y + 11, names[row]);
        createText(values[row], 7 + row * 2, 57, y, 124, y + 11, "--");
        UG_TextboxSetAlignment(&window_1, 7 + row * 2, ALIGN_CENTER_RIGHT);
        UG_TextboxSetForeColor(&window_1, 7 + row * 2, C_CYAN);
    }

    /* Button labels. */
    createText(&textbox_3, 2, 0, 144, 35, 159, "Hist");
    createText(&textbox_4, 3, 36, 144, 91, 159, "");
    createText(&textbox_5, 4, 92, 144, 127, 159, "Back");
    for (uint8_t id = 2; id <= 4; id++)
    {
        UG_TextboxSetBackColor(&window_1, id, C_RED);
    }
    UG_TextboxSetAlignment(&window_1, 4, ALIGN_CENTER_RIGHT);
    UG_WindowShow(&window_1);
}

static void render(void)
{
    char text[40];
    uint8_t id = systemInfo.currentDeviceId;
    DeviceInfo_t *device;
    uint8_t module;
    uint32_t seconds;

    ScrnCommon_StatusBarUpdate();
    if (id >= MAXIMUM_DEVICE || storageData.DeviceConfig[id].isDelete)
    {
        UG_TextboxSetText(&window_1, 5, "No paired device");
        for (uint8_t row = 0; row < INFO_ROWS; row++)
        {
            UG_TextboxSetText(&window_1, 7 + row * 2, "--");
        }
        UG_Update();
        return;
    }

    /* Title and identity. */
    device = &deviceBuff[id];
    module = systemInfo.currentModuleId;
    if (module >= sizeof(device->motorData) / sizeof(device->motorData[0]))
    {
        module = 0;
    }
    snprintf(text, sizeof(text), "%c%u%u Info: %.19s", storageData.DeviceConfig[id].factoryInfo.deviceTypeChar,
        device->deviceIndex + 1, module + 1, storageData.DeviceConfig[id].name);
    UG_TextboxSetText(&window_1, 5, text);
    snprintf(text, sizeof(text), "0x%08lX", (unsigned long)storageData.DeviceConfig[id].factoryInfo.serial);
    UG_TextboxSetText(&window_1, 7, text);
    snprintf(text, sizeof(text), "0x%02X", storageData.DeviceConfig[id].nodeIdConfig);
    UG_TextboxSetText(&window_1, 9, text);
    snprintf(text, sizeof(text), "%c", storageData.DeviceConfig[id].factoryInfo.deviceTypeChar);
    UG_TextboxSetText(&window_1, 11, text);
    snprintf(text, sizeof(text), "%u", storageData.DeviceConfig[id].factoryInfo.numberOfModule);
    UG_TextboxSetText(&window_1, 13, text);
    snprintf(text, sizeof(text), "%u", module + 1);
    UG_TextboxSetText(&window_1, 15, text);
    snprintf(text, sizeof(text), "%d dBm", device->rssi);
    UG_TextboxSetText(&window_1, 17, text);

    /* Age of the last packet. RTC can be edited; the link supervisor's monotonic age cannot underflow. */
    seconds = Link_Now() - linkDevices[id].lastRx;
    if (seconds < 100)
    {
        snprintf(text, sizeof(text), "%lu s", (unsigned long)seconds);
    }
    else if (seconds / 60 < 100)
    {
        snprintf(text, sizeof(text), "%lu min", (unsigned long)(seconds / 60));
    }
    else if (seconds / 3600 < 100)
    {
        snprintf(text, sizeof(text), "%lu h", (unsigned long)(seconds / 3600));
    }
    else
    {
        snprintf(text, sizeof(text), "%lu d", (unsigned long)(seconds / 86400));
    }
    UG_TextboxSetText(&window_1, 19, text);
    UG_Update();
}

/* Open this device's history list; long beep and stay here if it has none. */
static void leftClicked(void)
{
    if (!ScrnHs_OpenDeviceHistory(systemInfo.currentDeviceId))
    {
        Main_SetBuzzerMode(eBuzzer_Mode_LongBeep);
        return;
    }
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
}

static void leftPressed(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
}

/* Back to device control. */
static void rightClicked(void)
{
    Main_SetBuzzerMode(eBuzzer_Mode_ShortBeep);
    ScrnCommon_Switch(&deviceControlScreen);
}

static void noAction(void)
{
}

static void windowCallback(UG_MESSAGE *message)
{
    (void)message;
}

static void createText(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye, char *text)
{
    UG_TextboxCreate(&window_1, box, id, x, y, xe, ye);
    UG_TextboxSetFont(&window_1, id, DEFAULT_FONT);
    UG_TextboxSetText(&window_1, id, text);
    UG_TextboxSetBackColor(&window_1, id, C_BLACK);
    UG_TextboxSetForeColor(&window_1, id, C_WHITE);
    UG_TextboxSetAlignment(&window_1, id, ALIGN_CENTER_LEFT);
    UG_TextboxSetScrollEnabled(&window_1, id, 0);
}
