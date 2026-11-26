#include "lcd.h"
#include "screens.h"
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "config_store.h"

#define MANAGER_ROWS 4


/* Every paired device, including hidden ones. */
static uint8_t managerIds[MAXIMUM_DEVICE];
static uint8_t managerCount;
static uint8_t managerSelected;
static uint8_t managerPage = 0xFF;

static void init(void);
static void render(void);
static void leftClicked(void);
static void rightClicked(void);
static void centerClicked(void);
static void rightPressed(void);
static void noAction(void);
static void windowCallback(UG_MESSAGE *message);
static void createWindow(void);
static void createText(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye,
    char *text, UG_COLOR background);

/* LEFT/RIGHT select a device, CENTER toggles Hide/Show; hold LEFT, RIGHT or CENTER to go back. */
Screen_t deviceManagerScreen = {
    0,
    init,
    render,
    leftClicked,
    rightClicked,
    centerClicked,
    noAction,
    noAction,
    rightPressed,
    rightPressed,
    rightPressed,
    noAction,
    noAction
};

/**
 * @brief  Device highlighted in the device manager.
 * @return Device index, or MAXIMUM_DEVICE if none.
 */
uint8_t ScrnDvm_SelectedDevice(void)
{
    return managerSelected < managerCount ? managerIds[managerSelected] : MAXIMUM_DEVICE;
}

static void init(void)
{
    UG_TEXTBOX *rows[] = {&textbox_7, &textbox_9, &textbox_11, &textbox_13};

    /* List every paired device, including hidden ones. */
    managerCount = 0;
    managerSelected = 0;
    managerPage = 0xFF;
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        if (!storageData.devices[id].deleted)
        {
            managerIds[managerCount++] = id;
        }
    }

    createWindow();
    ScrnCommon_StatusBarCreate();
    createText(&textbox_6, 5, 0, 20, 127, 37, "Device manager", C_DARK_SLATE_BLUE);
    for (uint8_t row = 0; row < MANAGER_ROWS; row++)
    {
        createText(rows[row], 6 + row * 2, 0, 42 + row * 22, 127, 62 + row * 22, "", C_BLACK);
    }
    createText(&textbox_15, 20, 0, 130, 127, 141, "", C_BLACK);
    UG_TextboxSetScrollEnabled(&window_1, 20, 0);
    createText(&textbox_3, 2, 0, 144, 35, 159, "Up", C_RED);
    createText(&textbox_4, 3, 36, 144, 91, 159, "", C_RED);
    createText(&textbox_5, 4, 92, 144, 127, 159, "Down", C_RED);
    UG_WindowShow(&window_1);
}

static void render(void)
{
    uint8_t page = managerSelected / MANAGER_ROWS * MANAGER_ROWS;
    uint8_t pageChanged = page != managerPage;
    char text[96];
    uint8_t item;
    uint8_t id;
    const DeviceStorageData_t *device;
    uint8_t motors;

    if (pageChanged)
    {
        /* Clear stale rows once per page change, not on every render tick. */
        UG_FillFrame(0, 42, 127, 129, C_BLACK);
        managerPage = page;
    }

    /* Device rows. */
    for (uint8_t row = 0; row < MANAGER_ROWS; row++)
    {
        item = page + row;
        id = 6 + row * 2;
        if (item >= managerCount)
        {
            if (UG_TextboxCheckVisible(&window_1, id))
            {
                UG_TextboxHide(&window_1, id);
            }
            continue;
        }
        if (pageChanged || !UG_TextboxCheckVisible(&window_1, id))
        {
            UG_TextboxShow(&window_1, id);
        }
        device = &storageData.devices[managerIds[item]];
        motors = 0;
        for (uint8_t module = 0; module < MAXIMUM_MODULE; module++)
        {
            motors += Config_Motor(managerIds[item], module) != NULL;
        }
        /* A device row: "M<device>", its motor's name, or the motor count when it has several. */
        if (motors > 1)
        {
            snprintf(text, sizeof(text), "%c%u [%s] %u motors", device->factoryInfo.typeChar,
                managerIds[item] + 1, Config_DeviceHidden(managerIds[item]) ? "Hidden" : "Visible", motors);
        }
        else
        {
            snprintf(text, sizeof(text), "%c%u [%s] %.19s", device->factoryInfo.typeChar,
                managerIds[item] + 1, Config_DeviceHidden(managerIds[item]) ? "Hidden" : "Visible",
                ScrnCommon_MotorName(managerIds[item], 0));
        }
        UG_TextboxSetText(&window_1, id, text);
        UG_TextboxSetBackColor(&window_1, id, item == managerSelected ? C_DIM_GRAY : C_BLACK);
        UG_TextboxSetScrollEnabled(&window_1, id, item == managerSelected);
    }

    /* Page indicator and the centre button action. */
    if (managerCount)
    {
        snprintf(text, sizeof(text), "Hold: Back  %u-%u/%u", page + 1,
            page + MANAGER_ROWS < managerCount ? page + MANAGER_ROWS : managerCount, managerCount);
    }
    else
    {
        strcpy(text, "No paired devices");
    }
    UG_TextboxSetText(&window_1, 20, text);
    UG_TextboxSetText(&window_1, 3, !managerCount ? "" :
        Config_DeviceHidden(managerIds[managerSelected]) ? "Show" : "Hide");
    ScrnCommon_StatusBarUpdate();
    UG_Update();
}

/* Select the previous device. */
static void leftClicked(void)
{
    if (managerSelected)
    {
        managerSelected--;
    }
}

/* Select the next device. */
static void rightClicked(void)
{
    if (managerSelected + 1 < managerCount)
    {
        managerSelected++;
    }
}

/* Toggle Hide/Show of the selected device and save it. */
static void centerClicked(void)
{
    uint8_t id;

    if (!managerCount)
    {
        return;
    }
    id = managerIds[managerSelected];
    Config_SetDeviceHidden(id, !Config_DeviceHidden(id)); /* all motors of the device */
    ScrnCommon_SaveDeviceSettings();
}

/* Back to the main menu (also used for long LEFT and CENTER). */
static void rightPressed(void)
{
    ScrnCommon_Switch(&mainMenuScreen);
}

static void noAction(void)
{
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
