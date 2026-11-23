#ifndef SCREENS_H
#define SCREENS_H
#include "stdint.h"
#include "lcd.h"
#define MAX_OBJECTS 30
#define DEFAULT_FONT FONT_6X8

void Screen_ShowMotorLocked(uint8_t motorPower);
void Screen_ShowConnectionError(uint8_t deviceId);
uint8_t Screen_IsScheduleEditing(void);

#define PAIRING_STATE_SEARCHING  0
#define PAIRING_STATE_FOUND    1
#define PAIRING_STATE_NOT_FOUND 2
#define PAIRING_STATE_CONNECTING  3  
#define PAIRING_STATE_SUCCESS     4
#define PAIRING_STATE_FAILED   5
#define PAIRING_STATE_IDLE   6	

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


typedef struct {
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

extern Screen_t *currentScreen;
void Screen_EditCharactersCreate(uint8_t count);
void Screen_EditCharactersUpdate(uint8_t position);
void Screen_MenuScrollUpdate(uint8_t count, uint8_t selected);
void Screen_UpdateBattery(void);
void Screen_StatusBarCreate(void);
void Screen_StatusBarUpdate(void);
uint8_t Screen_DeviceVisible(uint8_t deviceId);
uint8_t Screen_EnsureVisibleDevice(void);
void Screen_SelectAdjacentVisible(int8_t direction);
void Screen_SaveDeviceFlags(uint8_t deviceId);
void Screen_SaveDeviceSettings(uint8_t deviceId);
void Screen_SaveRadioSettings(void);
void Screen_SaveGeneralSettings(void);
void Screen_EmptyControlInit(void);
void Screen_EmptyControlRender(void);
void Screen_OpenDeviceManager(void);
uint8_t Screen_ManagerSelectedDevice(void);
extern Screen_t deviceManagerScreen;
void Screen_SettingsMenuCreate(char *title);
void Screen_SettingsMenuCreateStacked(char *title);
void Screen_SettingsMenuPage(uint8_t page, uint8_t count);
void Screen_SettingsMenuBegin(uint8_t page);
void Screen_SettingsMenuRow(uint8_t row, char *label, char *value, uint8_t selected);
void Screen_SettingsMenuUpdate(void);
void Screen_GeneralSettingsResetSelection(void);
void Screen_DeviceSettingsResetSelection(void);
extern Screen_t deviceControlScreen;
extern Screen_t deviceSettingScreen;
extern Screen_t deviceMonitorScreen;
extern Screen_t editDeviceSettingScreen;
extern Screen_t mainMenuScreen;
extern Screen_t historyScreen;
void Screen_OpenHistory(void);
extern Screen_t generalSettingScreen;
extern Screen_t editGeneralSettingScreen;
extern Screen_t dateTimeScreen;
extern Screen_t devicePairingScreen;

extern uint8_t indexOfSettingMenu;
extern uint8_t indexOfGeneralSettingMenu;
extern uint8_t pairingUI_Step;
extern uint8_t historyScreenMode;
#endif
