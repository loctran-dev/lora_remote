#ifndef SCREENS_H
#define SCREENS_H
#include "stdint.h"
#include "lcd.h"

#define MAX_OBJECTS  30
#define DEFAULT_FONT FONT_6X8

#define PAIRING_STATE_SEARCHING  0
#define PAIRING_STATE_FOUND      1
#define PAIRING_STATE_NOT_FOUND  2
#define PAIRING_STATE_CONNECTING 3
#define PAIRING_STATE_SUCCESS    4
#define PAIRING_STATE_FAILED     5
#define PAIRING_STATE_IDLE       6

/* One screen: init/render plus a handler for each button click and long press. */
typedef struct
{
    uint8_t id;
    void (*init)(void);
    void (*render)(void);
    void (*leftButtonClickedCallback)(void);
    void (*rightButtonClickedCallback)(void);
    void (*centerButtonClickedCallback)(void);
    void (*bleftButtonClickedCallback)(void);
    void (*brightButtonClickedCallback)(void);
    void (*leftButtonPressedCallback)(void);
    void (*rightButtonPressedCallback)(void);
    void (*centerButtonPressedCallback)(void);
    void (*bleftButtonPressedCallback)(void);
    void (*brightButtonPressedCallback)(void);
} Screen_t;

/* uGUI objects shared by every screen (screens.c). */
extern UG_WINDOW window_1;
extern UG_OBJECT obj_buff_wnd_1[MAX_OBJECTS];
extern UG_TEXTBOX textbox_1;
extern UG_TEXTBOX textbox_2;
extern UG_TEXTBOX textbox_3;
extern UG_TEXTBOX textbox_4;
extern UG_TEXTBOX textbox_5;
extern UG_TEXTBOX textbox_6;
extern UG_TEXTBOX textbox_7;
extern UG_TEXTBOX textbox_8;
extern UG_TEXTBOX textbox_9;
extern UG_TEXTBOX textbox_10;
extern UG_TEXTBOX textbox_11;
extern UG_TEXTBOX textbox_12;
extern UG_TEXTBOX textbox_13;
extern UG_TEXTBOX textbox_14;
extern UG_TEXTBOX textbox_15;
extern UG_TEXTBOX textbox_16;
extern UG_TEXTBOX textbox_17;
extern UG_TEXTBOX textbox_18;
extern UG_TEXTBOX textbox_19;
extern UG_TEXTBOX textbox_20;
extern UG_IMAGE image_1;
extern UG_IMAGE image_2;
extern UG_IMAGE image_3;
extern UG_IMAGE image_4;
extern UG_GIF gif_1;
extern UG_GIF gif_2;
extern UG_REALEDIT real_edit_1;

/* Active screen (main.c) and the screen objects (screen_*.c, screens.c). */
extern Screen_t *currentScreen;
extern Screen_t mainMenuScreen;
extern Screen_t deviceControlScreen;
extern Screen_t deviceSettingScreen;
extern Screen_t editDeviceSettingScreen;
extern Screen_t deviceMonitorScreen;
extern Screen_t deviceManagerScreen;
extern Screen_t devicePairingScreen;
extern Screen_t generalSettingScreen;
extern Screen_t editGeneralSettingScreen;
extern Screen_t dateTimeScreen;
extern Screen_t historyScreen;

/* Screen state shared between screens (screen_*.c). */
extern uint8_t indexOfSettingMenu;
extern uint8_t indexOfGeneralSettingMenu;
extern uint8_t pairingUI_Step;
extern uint8_t historyScreenMode;

/* screens.c: screen switching */
void ScrnCommon_Switch(Screen_t *next);

/* screens.c: device visibility and selection */
uint8_t ScrnCommon_DeviceVisible(uint8_t deviceId);
uint8_t ScrnCommon_EnsureVisibleDevice(void);
void ScrnCommon_SelectAdjacentVisible(int8_t direction);

/* screens.c: settings persistence */
void ScrnCommon_SaveDeviceFlags(uint8_t deviceId);
void ScrnCommon_SaveDeviceSettings(uint8_t deviceId);
void ScrnCommon_SaveGeneralSettings(void);

/* screens.c: status bar */
void ScrnCommon_StatusBarCreate(void);
void ScrnCommon_StatusBarUpdate(void);
void ScrnCommon_UpdateBattery(void);

/* screens.c: settings menu */
void ScrnCommon_SettingsMenuCreate(char *title);
void ScrnCommon_SettingsMenuCreateStacked(char *title);
void ScrnCommon_SettingsMenuPage(uint8_t page, uint8_t count);
void ScrnCommon_SettingsMenuBegin(uint8_t page);
void ScrnCommon_SettingsMenuRow(uint8_t row, char *label, char *value, uint8_t selected);
void ScrnCommon_SettingsMenuUpdate(void);

/* screens.c: empty device control screen */
void ScrnCommon_EmptyControlInit(void);
void ScrnCommon_EmptyControlRender(void);

/* screens.c: menu scrolling and character editor */
void ScrnCommon_MenuScrollUpdate(uint8_t count, uint8_t selected);
void ScrnCommon_EditCharactersCreate(uint8_t count);
void ScrnCommon_EditCharactersUpdate(uint8_t position);

/* Implemented by individual screens (screen_*.c). */
uint8_t ScrnDvm_SelectedDevice(void);
void ScrnDcs_ShowMotorLocked(uint8_t motorPower);
void ScrnDcs_ShowConnectionError(uint8_t deviceId);
uint8_t ScrnDcs_IsScheduleEditing(void);
void ScrnDss_ResetSelection(void);
void ScrnGss_ResetSelection(void);
void ScrnHs_OpenHistory(void);
uint8_t ScrnHs_OpenDeviceHistory(uint8_t deviceId);

#endif
