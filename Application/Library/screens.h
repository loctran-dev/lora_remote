/**
 * @file    screens.h
 * @brief   Screen framework: the Screen_t table, shared uGUI objects and common screen helpers.
 *
 * Each screen_<xxx>.c defines one Screen_t with static handlers. systemTask
 * calls the button handlers of currentScreen, lcdTask calls its render()
 * every 200 ms, and ScrnCommon_Switch() changes screen. All screens draw into
 * the same window_1 and widget objects, guarded by uiMutex.
 *
 * Functions defined in screens.c use the ScrnCommon_ prefix; those defined in
 * screen_<xxx>.c use Scrn<Xxx>_.
 *
 * @defgroup screen Screens
 * @{
 */
#ifndef SCREENS_H
#define SCREENS_H
#include "stdint.h"
#include "lcd.h"

#define MAX_OBJECTS  30       ///< Widget slots in window_1.
#define DEFAULT_FONT FONT_6X8 ///< Font of the character editor.

/** @name devicePairingScreen steps (pairingUI_Step) */
/** @{ */
#define PAIRING_STATE_SEARCHING  0 ///< Waiting for a device to answer GET_FACTORY_INFO.
#define PAIRING_STATE_FOUND      1 ///< A device answered.
#define PAIRING_STATE_NOT_FOUND  2 ///< No device answered within 5 s.
#define PAIRING_STATE_CONNECTING 3 ///< CONFIG_CONNECTION sent, waiting for the reply.
#define PAIRING_STATE_SUCCESS    4 ///< Device paired and saved.
#define PAIRING_STATE_FAILED     5 ///< No reply, or no free device slot.
#define PAIRING_STATE_NO_ROOM    7 ///< Device table or motor pool full; the device was not contacted.
#define PAIRING_STATE_IDLE       6 ///< Not used.
/** @} */

/** @brief One screen: init/render plus a handler for each button click and long press. */
typedef struct
{
    uint8_t id;                                ///< Screen id.
    void (*init)(void);                        ///< Create the window and widgets.
    void (*render)(void);                      ///< Update the widgets; called every 200 ms outside SLEEP.
    void (*leftButtonClickedCallback)(void);   ///< LEFT click.
    void (*rightButtonClickedCallback)(void);  ///< RIGHT click.
    void (*centerButtonClickedCallback)(void); ///< CENTER click.
    void (*bleftButtonClickedCallback)(void);  ///< ON click.
    void (*brightButtonClickedCallback)(void); ///< OFF click.
    void (*leftButtonPressedCallback)(void);   ///< LEFT long press.
    void (*rightButtonPressedCallback)(void);  ///< RIGHT long press.
    void (*centerButtonPressedCallback)(void); ///< CENTER long press.
    void (*bleftButtonPressedCallback)(void);  ///< ON long press.
    void (*brightButtonPressedCallback)(void); ///< OFF long press.
} Screen_t;

/** @name uGUI objects shared by every screen (screens.c) */
/** @{ */
/** @brief Shared uGUI object; each screen's init() creates the ones it uses in window_1. */
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
/** @} */

/** @name Active screen (main.c) and the screen objects (screen_*.c) */
/** @{ */
extern Screen_t *currentScreen;            ///< Screen receiving buttons and render calls.
extern Screen_t mainMenuScreen;            ///< Main menu (screen_mms.c).
extern Screen_t deviceControlScreen;       ///< Device control, the start screen (screen_dcs.c).
extern Screen_t deviceSettingScreen;       ///< Device setup menu (screen_dss.c).
extern Screen_t editDeviceSettingScreen;   ///< Editor for one device setting (screen_edss.c).
extern Screen_t deviceMonitorScreen;       ///< Device info (screen_dms.c).
extern Screen_t deviceManagerScreen;       ///< Device manager (screen_dvm.c).
extern Screen_t devicePairingScreen;       ///< Pairing (screen_dps.c).
extern Screen_t generalSettingScreen;      ///< General setup menu (screen_gss.c).
extern Screen_t editGeneralSettingScreen;  ///< Editor for one general setting (screen_egss.c).
extern Screen_t dateTimeScreen;            ///< Date and time editor (screen_dts.c).
extern Screen_t historyScreen;             ///< History (screen_hs.c).
/** @} */

/** @name Screen state shared between screens (screen_*.c) */
/** @{ */
extern uint8_t indexOfSettingMenu;        ///< Device setting opened in editDeviceSettingScreen.
extern uint8_t indexOfGeneralSettingMenu; ///< General setting opened in editGeneralSettingScreen.
extern uint8_t pairingUI_Step;            ///< PAIRING_STATE_... step of devicePairingScreen.
extern uint8_t historyScreenMode;         ///< 0 = device picker, 1 = list, 2 = event detail.
/** @} */

/* screens.c: screen switching */

void ScrnCommon_Switch(Screen_t *next);

/* screens.c: device visibility and selection */

uint8_t ScrnCommon_DeviceVisible(uint8_t id);
uint8_t ScrnCommon_MotorVisible(uint8_t device, uint8_t module);
uint8_t ScrnCommon_SelectionVisible(void);
uint8_t ScrnCommon_EnsureVisibleDevice(void);
void ScrnCommon_SelectAdjacentVisible(int8_t direction);

/* screens.c: settings persistence */

MotorStorageData_t *ScrnCommon_CurrentMotor(void);
MotorData_t *ScrnCommon_CurrentMotorState(void);
const char *ScrnCommon_MotorName(uint8_t device, uint8_t module);
void ScrnCommon_SaveDeviceSettings(void);
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

/* Implemented by individual screens (screen_*.c) */

uint8_t ScrnDvm_SelectedDevice(void);
void ScrnDcs_ShowMotorLocked(uint8_t motorPower);
void ScrnDcs_ShowConnectionError(uint8_t id);
uint8_t ScrnDcs_IsScheduleEditing(void);
void ScrnDss_ResetSelection(void);
uint8_t ScrnEdss_CanEdit(uint8_t item);
void ScrnGss_ResetSelection(void);
void ScrnHs_OpenHistory(void);
uint8_t ScrnHs_OpenDeviceHistory(uint8_t deviceId);

#endif
/** @} */
