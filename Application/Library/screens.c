#include "lcd.h"
#include "screens.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "image_data.h"
#include "main.h"
extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];
extern SystemStorageData_t storageData;

/* Hide is device-level in the current single-module storage layout. */
uint8_t Screen_DeviceVisible(uint8_t id)
{
  return id < MAXIMUM_DEVICE && !deviceBuff[id].isDelete &&
    !deviceBuff[id].motorData[0].isHide;
}

uint8_t Screen_EnsureVisibleDevice(void)
{
  uint8_t start = systemInfo.currentDeviceId < MAXIMUM_DEVICE ? systemInfo.currentDeviceId : 0;
  /* Prefer the next device, then the closest preceding visible device. */
  for (int offset = 0; offset < MAXIMUM_DEVICE; offset++) {
    uint8_t id = start + offset < MAXIMUM_DEVICE ? start + offset :
      MAXIMUM_DEVICE - offset - 1;
    if (Screen_DeviceVisible(id)) {
      if (id != systemInfo.currentDeviceId || systemInfo.currentModuleId >= 6)
        systemInfo.currentModuleId = 0;
      systemInfo.currentDeviceId = id;
      return 1;
    }
  }
  /* Keep indices in range even when the control screen has no selection. */
  systemInfo.currentDeviceId = 0;
  systemInfo.currentModuleId = 0;
  return 0;
}

void Screen_SelectAdjacentVisible(int8_t direction)
{
  int id = systemInfo.currentDeviceId;
  for (id += direction; id >= 0 && id < MAXIMUM_DEVICE; id += direction) {
    if (Screen_DeviceVisible((uint8_t)id)) {
      systemInfo.currentDeviceId = id;
      systemInfo.currentModuleId = 0;
      return;
    }
  }
}

void Screen_SaveDeviceFlags(uint8_t id)
{
  if (id >= MAXIMUM_DEVICE) return;
  DeviceStorageData_t *stored = &storageData.DeviceConfig[id];
  MotorData_t *module = &deviceBuff[id].motorData[0];
  if (stored->isHide != module->isHide || stored->isLock != module->isLock) {
    stored->isHide = module->isHide;
    stored->isLock = module->isLock;
    systemInfo.isNeedSaveConfig = 1;
  }
}

void Screen_SaveDeviceSettings(uint8_t id)
{
  if (id >= MAXIMUM_DEVICE) return;
  DeviceStorageData_t *stored = &storageData.DeviceConfig[id];
  MotorData_t *module = &deviceBuff[id].motorData[0];
  stored->alarmThreshold = module->alarmThreshold;
  stored->alarmDelayTime = module->alarmDelayTime;
  stored->isLock = module->isLock;
  stored->isHide = module->isHide;
  stored->isDelete = deviceBuff[id].isDelete;
  memcpy(stored->name, module->name, sizeof(stored->name));
  stored->name[sizeof(stored->name) - 1] = 0;
  systemInfo.isNeedSaveConfig = 1;
}

void Screen_SaveRadioSettings(void)
{
  storageData.netIdConfig = systemInfo.netIdConfig;
  storageData.frequencyConfig = systemInfo.frequencyConfig;
  storageData.sfConfig = systemInfo.sfConfig;
  systemInfo.isNeedSaveConfig = 1;
}

void Screen_SaveGeneralSettings(void)
{
  Screen_SaveRadioSettings();
  storageData.sleepMode = systemInfo.sleepMode;
  storageData.tUpdate = systemInfo.tUpdate;
  storageData.tSleepUpdate = systemInfo.tSleepUpdate;
  Main_UpdateStatusPeriod();
}

static uint8_t managerIds[MAXIMUM_DEVICE];
static uint8_t managerCount, managerSelected;
static uint8_t managerPage = 0xFF;

uint8_t Screen_ManagerSelectedDevice(void)
{
  return managerSelected < managerCount ? managerIds[managerSelected] : MAXIMUM_DEVICE;
}

static void Simple_Callback(UG_MESSAGE *message) { (void)message; }
static void Simple_NoAction(void) {}

static void Simple_Window(void)
{
  UG_WindowCreate(&window_1, obj_buff_wnd_1, MAX_OBJECTS, Simple_Callback);
  UG_WindowSetStyle(&window_1, WND_STYLE_HIDE_TITLE);
  UG_WindowSetTitleHeight(&window_1, 0);
  UG_WindowSetBackColor(&window_1, C_BLACK);
  UG_WindowSetXStart(&window_1, 0); UG_WindowSetYStart(&window_1, 0);
  UG_WindowSetXEnd(&window_1, LCD_WIDTH - 1); UG_WindowSetYEnd(&window_1, LCD_HEIGHT - 1);
}

static void Simple_Text(UG_TEXTBOX *box, uint8_t id, int x, int y, int xe, int ye,
                        char *text, UG_COLOR background)
{
  UG_TextboxCreate(&window_1, box, id, x, y, xe, ye);
  UG_TextboxSetFont(&window_1, id, DEFAULT_FONT);
  UG_TextboxSetText(&window_1, id, text);
  UG_TextboxSetBackColor(&window_1, id, background);
  UG_TextboxSetForeColor(&window_1, id, C_WHITE);
  UG_TextboxSetAlignment(&window_1, id, ALIGN_CENTER_LEFT);
}

/* Every screen reserves textbox 0/1 and image 0 for this same status bar.
   Bitmap bounds (15x9) do not overlap the four-character 100% textbox. */
void Screen_StatusBarCreate(void)
{
  Simple_Text(&textbox_1, 0, 0, 0, 50, 15, "", C_BLACK);
  Simple_Text(&textbox_2, 1, 104, 0, 127, 15, "", C_BLACK);
  UG_TextboxSetAlignment(&window_1, 1, ALIGN_CENTER_RIGHT);
  UG_TextboxSetScrollEnabled(&window_1, 0, 0);
  UG_TextboxSetScrollEnabled(&window_1, 1, 0);
  UG_ImageCreate(&window_1, &image_1, IMG_ID_0, 87, 3, 101, 11);
  Screen_StatusBarUpdate();
}
void Screen_StatusBarUpdate(void)
{
  char time[8];
  sprintf(time, "%02u:%02u", systemInfo.time.Hours, systemInfo.time.Minutes);
  UG_TextboxSetText(&window_1, 0, time);
  Screen_UpdateBattery();
}

static uint8_t settingsPage, settingsPageChanged, settingsRows, settingsStacked;
static void Settings_MenuCreate(char *title, uint8_t stacked)
{
  UG_TEXTBOX *labels[] = {&textbox_7, &textbox_9, &textbox_11, &textbox_13, &textbox_15};
  UG_TEXTBOX *values[] = {&textbox_8, &textbox_10, &textbox_12, &textbox_14, &textbox_16};
  settingsPage = 255;
  settingsStacked = stacked;
  settingsRows = stacked ? 4 : 5;
  Simple_Window();
  Screen_StatusBarCreate();
  Simple_Text(&textbox_6, 20, 0, 20, stacked ? 99 : 127, 37, title, C_DARK_SLATE_BLUE);
  if (stacked) {
    Simple_Text(&textbox_17, 21, 100, 20, 127, 37, "", C_DARK_SLATE_BLUE);
    UG_TextboxSetAlignment(&window_1, 21, ALIGN_CENTER);
    UG_TextboxSetScrollEnabled(&window_1, 21, 0);
  }
  for (uint8_t row = 0; row < settingsRows; row++) {
    if (stacked) {
      int y = 42 + row * 24;
      Simple_Text(labels[row], 6 + row * 2, 3, y, 124, y + 10, "", C_BLACK);
      Simple_Text(values[row], 7 + row * 2, 3, y + 11, 124, y + 21, "", C_BLACK);
      UG_TextboxSetAlignment(&window_1, 7 + row * 2, ALIGN_CENTER_RIGHT);
      UG_TextboxSetForeColor(&window_1, 7 + row * 2, C_CYAN);
      UG_TextboxSetScrollEnabled(&window_1, 7 + row * 2, 0);
    } else {
      Simple_Text(labels[row], 6 + row * 2, 0, 42 + row * 18, 80, 58 + row * 18, "", C_BLACK);
      Simple_Text(values[row], 7 + row * 2, 81, 42 + row * 18, 127, 58 + row * 18, "", C_BLACK);
    }
  }
  Simple_Text(&textbox_3, 2, 0, 144, 35, 159, "Up", C_RED);
  Simple_Text(&textbox_4, 3, 36, 144, 91, 159, "Change", C_RED);
  Simple_Text(&textbox_5, 4, 92, 144, 127, 159, "Down", C_RED);
  UG_WindowShow(&window_1);
}
void Screen_SettingsMenuCreate(char *title) { Settings_MenuCreate(title, 0); }
void Screen_SettingsMenuCreateStacked(char *title) { Settings_MenuCreate(title, 1); }
void Screen_SettingsMenuPage(uint8_t page, uint8_t count)
{
  if (settingsStacked) {
    char text[12];
    sprintf(text, "%u/%u", page / settingsRows + 1, (count + settingsRows - 1) / settingsRows);
    UG_TextboxSetText(&window_1, 21, text);
  }
}
void Screen_SettingsMenuBegin(uint8_t page)
{
  settingsPageChanged = page != settingsPage;
  if (settingsPageChanged) {
    UG_FillFrame(0, 42, 127, settingsStacked ? 139 : 132, C_BLACK);
    settingsPage = page;
  }
}
void Screen_SettingsMenuRow(uint8_t row, char *label, char *value, uint8_t selected)
{
  if (row >= settingsRows) return;
  for (uint8_t column = 0; column < 2; column++) {
    uint8_t id = 6 + row * 2 + column;
    if (!label) {
      if (UG_TextboxCheckVisible(&window_1, id)) UG_TextboxHide(&window_1, id);
      continue;
    }
    if (settingsPageChanged || !UG_TextboxCheckVisible(&window_1, id)) UG_TextboxShow(&window_1, id);
    UG_TextboxSetText(&window_1, id, column ? value : label);
    UG_TextboxSetBackColor(&window_1, id, selected ? C_DIM_GRAY : C_BLACK);
    UG_TextboxSetScrollEnabled(&window_1, id, settingsStacked && column ? 0 : selected);
  }
}
void Screen_SettingsMenuUpdate(void)
{
  Screen_StatusBarUpdate();
  UG_Update();
}

void Screen_EmptyControlInit(void)
{
  Simple_Window();
  Screen_StatusBarCreate();
  Simple_Text(&textbox_6, 5, 0, 20, 127, 37, "Device control", C_DARK_SLATE_BLUE);
  Simple_Text(&textbox_7, 6, 0, 48, 127, 65, "No visible devices", C_BLACK);
  Simple_Text(&textbox_8, 8, 0, 76, 127, 93, "Use Device manager", C_BLACK);
  Simple_Text(&textbox_9, 10, 0, 94, 127, 111, "to show devices", C_BLACK);
  Simple_Text(&textbox_4, 3, 0, 144, 127, 159, "Device manager", C_RED);
  UG_WindowShow(&window_1);
  UG_Update();
}

void Screen_EmptyControlRender(void) { Screen_StatusBarUpdate(); UG_Update(); }

void Screen_OpenDeviceManager(void)
{
  UG_WindowHide(&window_1); UG_WindowDelete(&window_1);
  currentScreen = &deviceManagerScreen;
  currentScreen->init(); currentScreen->render();
}

static void Manager_Render(void)
{
  uint8_t page = managerSelected / 4 * 4;
  uint8_t pageChanged = page != managerPage;
  char text[96];
  if (pageChanged) {
    /* Clear stale rows once per page change, not on every render tick. */
    UG_FillFrame(0, 42, 127, 129, C_BLACK);
    managerPage = page;
  }
  for (uint8_t row = 0; row < 4; row++) {
    uint8_t item = page + row;
    uint8_t id = 6 + row * 2;
    if (item >= managerCount) {
      if (UG_TextboxCheckVisible(&window_1, id)) UG_TextboxHide(&window_1, id);
      continue;
    }
    if (pageChanged || !UG_TextboxCheckVisible(&window_1, id)) UG_TextboxShow(&window_1, id);
    if (item < managerCount) {
      DeviceInfo_t *device = &deviceBuff[managerIds[item]];
      snprintf(text, sizeof(text), "%c%u1 [%s] %.19s",
        device->factoryInfo.deviceTypeChar, device->deviceIndex + 1,
        device->motorData[0].isHide ? "Hidden" : "Visible", device->motorData[0].name);
    } else text[0] = 0;
    UG_TextboxSetText(&window_1, id, text);
    UG_TextboxSetBackColor(&window_1, id,
      item < managerCount && item == managerSelected ? C_DIM_GRAY : C_BLACK);
    UG_TextboxSetScrollEnabled(&window_1, id, item < managerCount && item == managerSelected);
  }
  if (managerCount)
    snprintf(text, sizeof(text), "Hold: Back  %u-%u/%u", page + 1,
      page + 4 < managerCount ? page + 4 : managerCount, managerCount);
  else strcpy(text, "No paired devices");
  UG_TextboxSetText(&window_1, 20, text);
  UG_TextboxSetText(&window_1, 3, !managerCount ? "" :
    deviceBuff[managerIds[managerSelected]].motorData[0].isHide ? "Show" : "Hide");
  Screen_StatusBarUpdate();
  UG_Update();
}

static void Manager_Init(void)
{
  UG_TEXTBOX *rows[] = {&textbox_7, &textbox_9, &textbox_11, &textbox_13};
  managerCount = 0; managerSelected = 0; managerPage = 0xFF;
  for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    if (!deviceBuff[id].isDelete) managerIds[managerCount++] = id;
  Simple_Window();
  Screen_StatusBarCreate();
  Simple_Text(&textbox_6, 5, 0, 20, 127, 37, "Device manager", C_DARK_SLATE_BLUE);
  for (uint8_t row = 0; row < 4; row++)
    Simple_Text(rows[row], 6 + row * 2, 0, 42 + row * 22, 127, 62 + row * 22, "", C_BLACK);
  Simple_Text(&textbox_15, 20, 0, 130, 127, 141, "", C_BLACK);
  UG_TextboxSetScrollEnabled(&window_1, 20, 0);
  Simple_Text(&textbox_3, 2, 0, 144, 35, 159, "Up", C_RED);
  Simple_Text(&textbox_4, 3, 36, 144, 91, 159, "", C_RED);
  Simple_Text(&textbox_5, 4, 92, 144, 127, 159, "Down", C_RED);
  UG_WindowShow(&window_1);
}

static void Manager_Previous(void) { if (managerSelected) managerSelected--; }
static void Manager_Next(void) { if (managerSelected + 1 < managerCount) managerSelected++; }
static void Manager_Toggle(void)
{
  if (!managerCount) return;
  uint8_t id = managerIds[managerSelected];
  deviceBuff[id].motorData[0].isHide = !deviceBuff[id].motorData[0].isHide;
  Screen_SaveDeviceFlags(id);
}
static void Manager_Back(void)
{
  UG_WindowHide(&window_1); UG_WindowDelete(&window_1);
  currentScreen = &mainMenuScreen;
  currentScreen->init(); currentScreen->render();
}
Screen_t deviceManagerScreen = {
  0, Manager_Init, Manager_Render,
  Manager_Previous, Manager_Next, Manager_Toggle, Simple_NoAction, Simple_NoAction,
  Manager_Back, Manager_Back, Manager_Back, Simple_NoAction, Simple_NoAction
};

void Screen_UpdateBattery(void)
{
  char text[8];
  uint8_t valid = systemInfo.batteryVoltage > 0;
  if (valid) sprintf(text, "%u%%", systemInfo.batteryUsage);
  else strcpy(text, "--%");
  UG_TextboxSetText(&window_1, TXB_ID_1, text);
  UG_TextboxSetScrollEnabled(&window_1, TXB_ID_1, 0);
  UG_TextboxSetForeColor(&window_1, TXB_ID_1,
    valid && systemInfo.batteryUsage <= 10 ? C_RED : C_WHITE);
  uint8_t frame = !valid ? 0 : systemInfo.batteryUsage <= 5 ? 0 :
    systemInfo.batteryUsage <= 25 ? 1 : systemInfo.batteryUsage <= 50 ? 2 :
    systemInfo.batteryUsage <= 75 ? 3 : 4;
  if (systemInfo.isChanging && valid) frame = (HAL_GetTick() / 500) % 5;
  UG_ImageSetBMP(&window_1, IMG_ID_0, batteryGIF[frame]);
}

static UG_TEXTBOX editCharacters[19];
static uint8_t editCharacterCount;

void Screen_MenuScrollUpdate(uint8_t count, uint8_t selected)
{
  for (uint8_t i = 0; i < count; i++)
  {
    UG_TextboxSetScrollEnabled(&window_1, TXB_ID_6 + i * 2, i == selected);
    UG_TextboxSetScrollEnabled(&window_1, TXB_ID_7 + i * 2, i == selected);
  }
}

void Screen_EditCharactersCreate(uint8_t count)
{
  editCharacterCount = count > 19 ? 19 : count;
  UG_TextboxHide(&window_1, TXB_ID_7);
  for (uint8_t i = 0; i < editCharacterCount; i++)
  {
    uint8_t columns = count > 10 ? 10 : count;
    uint16_t x = count > 10 ? 4 + (i % 10) * 12 : (128 - columns * 12) / 2 + i * 12;
    uint16_t y = 57 + (i / columns) * 20;
    UG_TextboxCreate(&window_1, &editCharacters[i], 32 + i, x, y, x + 10, y + 16);
    UG_TextboxSetFont(&window_1, 32 + i, DEFAULT_FONT);
    UG_TextboxSetAlignment(&window_1, 32 + i, ALIGN_CENTER);
  }
}

void Screen_EditCharactersUpdate(uint8_t position)
{
  char *text = UG_TextboxGetText(&window_1, TXB_ID_7);
  if (text == NULL) return;
  uint8_t length = strlen(text);
  for (uint8_t i = 0; i < editCharacterCount; i++)
  {
    char character[2] = {i < length ? text[i] : ' ', '\0'};
    UG_TextboxSetText(&window_1, 32 + i, character);
    UG_TextboxSetBackColor(&window_1, 32 + i, i == position ? C_YELLOW : C_BLACK);
    UG_TextboxSetForeColor(&window_1, 32 + i, i == position ? C_BLACK : C_WHITE);
  }
  free(text);
}

UG_WINDOW window_1;
UG_OBJECT obj_buff_wnd_1[MAX_OBJECTS];
UG_TEXTBOX textbox_1;
UG_TEXTBOX textbox_2;
UG_TEXTBOX textbox_3;
UG_TEXTBOX textbox_4;
UG_TEXTBOX textbox_5;
UG_TEXTBOX textbox_6;
UG_TEXTBOX textbox_7;
UG_TEXTBOX textbox_8;
UG_TEXTBOX textbox_9;
UG_TEXTBOX textbox_10;
UG_TEXTBOX textbox_11;
UG_TEXTBOX textbox_12;
UG_TEXTBOX textbox_13;
UG_TEXTBOX textbox_14;
UG_TEXTBOX textbox_15;
UG_TEXTBOX textbox_16;
UG_TEXTBOX textbox_17;
UG_TEXTBOX textbox_18;
UG_TEXTBOX textbox_19;
UG_TEXTBOX textbox_20;
UG_IMAGE image_1;
UG_IMAGE image_2;
UG_IMAGE image_3;
UG_IMAGE image_4;
UG_GIF gif_1;
UG_GIF gif_2;
UG_REALEDIT real_edit_1;
