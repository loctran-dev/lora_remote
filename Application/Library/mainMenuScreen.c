#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];


static uint8_t menuIndex;
static void window_1_callback(UG_MESSAGE *msg)
{
}
void MMS_LeftButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	if (menuIndex > 0)
	{
		menuIndex--;

		uint8_t txbidMap[] = {TXB_ID_6, TXB_ID_8, TXB_ID_10, TXB_ID_12, TXB_ID_14};
		
		UG_TextboxSetBackColor(&window_1, txbidMap[menuIndex], C_DIM_GRAY);
		
		if (txbidMap[menuIndex] < TXB_ID_14)
		{
			UG_TextboxSetBackColor(&window_1, txbidMap[menuIndex] + 2, C_BLACK);
		}
		
		if (txbidMap[menuIndex] > TXB_ID_6) 
		{
			UG_TextboxSetBackColor(&window_1, txbidMap[menuIndex] - 2, C_BLACK);
		}
			
		Screen_MenuScrollUpdate(5, menuIndex);
		UG_Update();
	}
}
void MMS_LeftButtonPressedCallback(void)
{
}
void MMS_RightButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	if (menuIndex < 4)
	{
		menuIndex++;
		uint8_t txbidMap[] = {TXB_ID_6, TXB_ID_8, TXB_ID_10, TXB_ID_12, TXB_ID_14};
		
		UG_TextboxSetBackColor(&window_1, txbidMap[menuIndex], C_DIM_GRAY);		
		if (txbidMap[menuIndex] < TXB_ID_14)
		{
			UG_TextboxSetBackColor(&window_1, txbidMap[menuIndex] + 2, C_BLACK);
		}
		
		if (txbidMap[menuIndex] > TXB_ID_6) 
		{
			UG_TextboxSetBackColor(&window_1, txbidMap[menuIndex] - 2, C_BLACK);
		}
		Screen_MenuScrollUpdate(5, menuIndex);
		UG_Update();
	}
}
void MMS_RightButtonPressedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
}
void MMS_CenterButtonClickedCallback(void)
{
    if (menuIndex==4) { Screen_OpenHistory(); return; }
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	
	if (menuIndex == 0)
	{
		Screen_OpenDeviceManager();
	}
	else if (menuIndex == 1)
	{
		UG_WindowHide(&window_1);
		UG_WindowDelete(&window_1);
		currentScreen = &deviceControlScreen;
		currentScreen->init();
		currentScreen->render();
	}
	else 	if (menuIndex == 2) 
	{
		UG_WindowHide(&window_1);
		UG_WindowDelete(&window_1);
		currentScreen = &generalSettingScreen;
		Screen_GeneralSettingsResetSelection();
		currentScreen->init();
		currentScreen->render();
	}
	else 	if (menuIndex == 3) 
	{
		pairingUI_Step = PAIRING_STATE_SEARCHING;
		UG_WindowHide(&window_1);
		UG_WindowDelete(&window_1);
		currentScreen = &devicePairingScreen;
		currentScreen->init();
		currentScreen->render();
		
		Main_ScanNewDevice();
	}

}
void MMS_CenterButtonPressedCallback(void)
{
}
void MMS_BLeftButtonClickedCallback(void)
{
}
void MMS_BLeftButtonPressedCallback(void)
{
}
void MMS_BRightButtonClickedCallback(void)
{
}
void MMS_BRightButtonPressedCallback(void)
{
}
void MMS_Init(void)
{
	// Create the window
	UG_WindowCreate(&window_1, obj_buff_wnd_1, MAX_OBJECTS, window_1_callback);
	UG_WindowSetTitleText(&window_1, "");
	UG_WindowSetTitleTextFont(&window_1, DEFAULT_FONT);
	UG_WindowSetTitleHeight(&window_1, 0);
	UG_WindowSetStyle(&window_1, WND_STYLE_HIDE_TITLE);
	UG_WindowSetBackColor(&window_1, C_BLACK);
	UG_WindowSetXStart(&window_1, 0);
	UG_WindowSetYStart(&window_1, 0);
	UG_WindowSetXEnd(&window_1, LCD_WIDTH-1);
	UG_WindowSetYEnd(&window_1, LCD_HEIGHT-1);

// Create Textbox time
	Screen_StatusBarCreate();
		
	// Create Textbox left button
	UG_TextboxCreate(&window_1, &textbox_3, TXB_ID_2, 0, 144, 43, 159);
	UG_TextboxSetFont(&window_1, TXB_ID_2, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_2, " Up");
	UG_TextboxSetBackColor(&window_1, TXB_ID_2, C_RED);
	UG_TextboxSetForeColor(&window_1, TXB_ID_2, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_2, ALIGN_CENTER_LEFT);
	
	// Create Textbox center button
	UG_TextboxCreate(&window_1, &textbox_4, TXB_ID_3, 44, 144, 83, 159);
	UG_TextboxSetFont(&window_1, TXB_ID_3, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_3, "Go to");
	UG_TextboxSetBackColor(&window_1, TXB_ID_3, C_RED);
	UG_TextboxSetForeColor(&window_1, TXB_ID_3, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_3, ALIGN_CENTER);

	// Create Textbox rigth button
	UG_TextboxCreate(&window_1, &textbox_5, TXB_ID_4, 84, 144, 127, 159);
	UG_TextboxSetFont(&window_1, TXB_ID_4, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_4, "Down ");
	UG_TextboxSetBackColor(&window_1, TXB_ID_4, C_RED);
	UG_TextboxSetForeColor(&window_1, TXB_ID_4, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_4, ALIGN_CENTER_RIGHT);
	
	// Create Textbox 
	char title[30];
	sprintf(title, "Menu");
	UG_TextboxCreate(&window_1, &textbox_6, TXB_ID_5, 0, 20, 127, 37);
	UG_TextboxSetFont(&window_1, TXB_ID_5, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_5, title);
	UG_TextboxSetBackColor(&window_1, TXB_ID_5, C_DARK_SLATE_BLUE);
	UG_TextboxSetForeColor(&window_1, TXB_ID_5, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_5, ALIGN_CENTER_LEFT);
	
	uint16_t x1, x2, y1, y2, dy1, dy2;
	x1 = 15;
	x2 = 120;
	dy1 = 10;
	dy2 = 3;
	
	y1 = 42;
	y2 = y1 + dy1;
	
	UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, x1, y1, x2, y2);
	UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_6, "Device manager");
	UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
	UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_LEFT);
	
	y1 = y2 + dy2;
	y2 = y1 + dy1;
	UG_TextboxCreate(&window_1, &textbox_9, TXB_ID_8, x1, y1, x2, y2);
	UG_TextboxSetFont(&window_1, TXB_ID_8, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_8, "Device control");
	UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
	UG_TextboxSetForeColor(&window_1, TXB_ID_8, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_8, ALIGN_CENTER_LEFT);
	
	y1 = y2 + dy2;
	y2 = y1 + dy1;
	UG_TextboxCreate(&window_1, &textbox_11, TXB_ID_10, x1, y1, x2, y2);
	UG_TextboxSetFont(&window_1, TXB_ID_10, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_10, "General setting");
	UG_TextboxSetBackColor(&window_1, TXB_ID_10, C_BLACK);
	UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_10, ALIGN_CENTER_LEFT);

	y1 = y2 + dy2;
	y2 = y1 + dy1;
	UG_TextboxCreate(&window_1, &textbox_13, TXB_ID_12, x1, y1, x2, y2);
	UG_TextboxSetFont(&window_1, TXB_ID_12, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_12, "Find new device");
	UG_TextboxSetBackColor(&window_1, TXB_ID_12, C_BLACK);
	UG_TextboxSetForeColor(&window_1, TXB_ID_12, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_12, ALIGN_CENTER_LEFT);
	
    UG_TextboxCreate(&window_1,&textbox_15,TXB_ID_14,x1,y2+dy2,x2,y2+dy2+dy1);
    UG_TextboxSetFont(&window_1,TXB_ID_14,DEFAULT_FONT);
    UG_TextboxSetText(&window_1,TXB_ID_14,"History");
    UG_TextboxSetBackColor(&window_1,TXB_ID_14,C_BLACK);
    UG_TextboxSetForeColor(&window_1,TXB_ID_14,C_WHITE);
    UG_TextboxSetAlignment(&window_1,TXB_ID_14,ALIGN_CENTER_LEFT);
	UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_DIM_GRAY);
	menuIndex = 0;
	
	Screen_MenuScrollUpdate(5, menuIndex);
	UG_WindowShow(&window_1);
	UG_Update();
}
void MMS_Render(void)
{
	Screen_MenuScrollUpdate(5, menuIndex);
	Screen_StatusBarUpdate();
	UG_Update();
}
Screen_t mainMenuScreen = 
{ 
	0,
	MMS_Init,
	MMS_Render,
	MMS_LeftButtonClickedCallback,
	MMS_RightButtonClickedCallback,
	MMS_CenterButtonClickedCallback,
	MMS_BLeftButtonClickedCallback,
	MMS_BRightButtonClickedCallback,
	MMS_LeftButtonPressedCallback,
	MMS_RightButtonPressedCallback,
	MMS_CenterButtonPressedCallback,
	MMS_BLeftButtonPressedCallback,
	MMS_BRightButtonPressedCallback
};
