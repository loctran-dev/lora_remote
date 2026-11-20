#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];


uint8_t pairingUI_Step;
	
static void window_1_callback(UG_MESSAGE *msg)
{
}
void DPS_LeftButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);

	if (pairingUI_Step == PAIRING_STATE_SEARCHING || 
		pairingUI_Step == PAIRING_STATE_CONNECTING ||
		pairingUI_Step == PAIRING_STATE_SUCCESS) 
	{		
	}
	else if(pairingUI_Step == PAIRING_STATE_FOUND ||
		pairingUI_Step == PAIRING_STATE_NOT_FOUND)
	{
		pairingUI_Step = PAIRING_STATE_SEARCHING;
		UG_WindowHide(&window_1);
		UG_WindowDelete(&window_1);
		currentScreen = &devicePairingScreen;
		currentScreen->init();
		currentScreen->render();
		
		Main_ScanNewDevice();
	}
	else if(pairingUI_Step == PAIRING_STATE_FAILED)
	{
		pairingUI_Step = PAIRING_STATE_CONNECTING;
		UG_WindowHide(&window_1);
		UG_WindowDelete(&window_1);
		currentScreen = &devicePairingScreen;
		currentScreen->init();
		currentScreen->render();
		
		Main_ConnectToDevice();
	}
}
void DPS_LeftButtonPressedCallback(void)
{
}
void DPS_RightButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	if (pairingUI_Step == PAIRING_STATE_SEARCHING || 
		pairingUI_Step == PAIRING_STATE_FOUND || 
		pairingUI_Step == PAIRING_STATE_NOT_FOUND ||
		pairingUI_Step == PAIRING_STATE_CONNECTING || 
		pairingUI_Step == PAIRING_STATE_SUCCESS || 
		pairingUI_Step == PAIRING_STATE_FAILED) 
	{
		UG_WindowHide(&window_1);
		UG_WindowDelete(&window_1);
		currentScreen = &mainMenuScreen;
		currentScreen->init();
		currentScreen->render();
	}
}
void DPS_RightButtonPressedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	
	UG_WindowHide(&window_1);
  UG_WindowDelete(&window_1);
	currentScreen = &mainMenuScreen;
	currentScreen->init();
	currentScreen->render();
}
void DPS_CenterButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	if (pairingUI_Step == PAIRING_STATE_SEARCHING) 
	{
	}
	else if(pairingUI_Step == PAIRING_STATE_FOUND || pairingUI_Step == PAIRING_STATE_FAILED)
	{
		pairingUI_Step = PAIRING_STATE_CONNECTING;
		UG_WindowHide(&window_1);
		UG_WindowDelete(&window_1);
		currentScreen = &devicePairingScreen;
		currentScreen->init();
		currentScreen->render();
		
		Main_ConnectToDevice();
	}
	else if(pairingUI_Step == PAIRING_STATE_NOT_FOUND)
	{
	}
	else if(pairingUI_Step == PAIRING_STATE_CONNECTING)
	{
	}
	else if(pairingUI_Step == PAIRING_STATE_SUCCESS)
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
void DPS_CenterButtonPressedCallback(void)
{
}
void DPS_BLeftButtonClickedCallback(void)
{
}
void DPS_BLeftButtonPressedCallback(void)
{
}
void DPS_BRightButtonClickedCallback(void)
{
}
void DPS_BRightButtonPressedCallback(void)
{
}
void DPS_Init(void)
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
		
	/* Each pairing state creates its own footer once, below. */
	
		// Create Textbox 
	char title[30];
	sprintf(title, "Device pairing");
	UG_TextboxCreate(&window_1, &textbox_6, TXB_ID_5, 0, 20, 127, 37);
	UG_TextboxSetFont(&window_1, TXB_ID_5, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_5, title);
	UG_TextboxSetBackColor(&window_1, TXB_ID_5, C_DARK_SLATE_BLUE);
	UG_TextboxSetForeColor(&window_1, TXB_ID_5, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_5, ALIGN_CENTER_LEFT);
	
	if (pairingUI_Step == PAIRING_STATE_SEARCHING) 
	{
		// Create Textbox left button
		UG_TextboxCreate(&window_1, &textbox_3, TXB_ID_2, 0, 144, 43, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_2, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_2, "");
		UG_TextboxSetBackColor(&window_1, TXB_ID_2, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_2, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_2, ALIGN_CENTER_LEFT);
		
		// Create Textbox center button
		UG_TextboxCreate(&window_1, &textbox_4, TXB_ID_3, 44, 144, 83, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_3, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_3, "");
		UG_TextboxSetBackColor(&window_1, TXB_ID_3, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_3, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_3, ALIGN_CENTER);

		// Create Textbox rigth button
		UG_TextboxCreate(&window_1, &textbox_5, TXB_ID_4, 84, 144, 127, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_4, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_4, "Cancel ");
		UG_TextboxSetBackColor(&window_1, TXB_ID_4, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_4, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_4, ALIGN_CENTER_RIGHT);
		
		UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 15, 72, 123, 82);
		UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_6, "Scanning...");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER);
	}
	else if(pairingUI_Step == PAIRING_STATE_FOUND)
	{
				// Create Textbox left button
		UG_TextboxCreate(&window_1, &textbox_3, TXB_ID_2, 0, 144, 43, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_2, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_2, "Scan");
		UG_TextboxSetBackColor(&window_1, TXB_ID_2, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_2, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_2, ALIGN_CENTER_LEFT);
		
		// Create Textbox center button
		UG_TextboxCreate(&window_1, &textbox_4, TXB_ID_3, 44, 144, 83, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_3, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_3, "Conn");
		UG_TextboxSetBackColor(&window_1, TXB_ID_3, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_3, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_3, ALIGN_CENTER);

		// Create Textbox rigth button
		UG_TextboxCreate(&window_1, &textbox_5, TXB_ID_4, 84, 144, 127, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_4, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_4, "Exit ");
		UG_TextboxSetBackColor(&window_1, TXB_ID_4, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_4, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_4, ALIGN_CENTER_RIGHT);
		
		UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 15, 52, 123, 62);
		UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_6, "Device found!");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER);
		
		char snText[20];
		char typeText[20];
		char moduleCountText[20];
		snprintf(snText, sizeof(snText), "0x%08lX", (unsigned long)systemInfo.findDevicefactoryInfo.serial);
		sprintf(typeText, "%c", systemInfo.findDevicefactoryInfo.deviceTypeChar);
		sprintf(moduleCountText, "%02d", systemInfo.findDevicefactoryInfo.numberOfModule);
	
		uint16_t x1, x2, x3, x4, y1, y2, dy1, dy2;
		x1 = 3; x2 = 55; x3 = 57; x4 = 124;	dy1 = 10;	dy2 = 3;		
		y1 = 80;
		y2 = y1 + dy1;
	
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7, "SN");
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER_LEFT);

		UG_TextboxCreate(&window_1, &textbox_9, TXB_ID_8, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_8, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_8, snText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_8, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_8, ALIGN_CENTER_RIGHT);
		
		y1 = y2 + dy2;
		y2 = y1 + dy1;
		UG_TextboxCreate(&window_1, &textbox_10, TXB_ID_9, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_9, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_9, "Type");
		UG_TextboxSetBackColor(&window_1, TXB_ID_9, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_9, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_9, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_11, TXB_ID_10, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_10, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_10, typeText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_10, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_10, ALIGN_CENTER_RIGHT);

		y1 = y2 + dy2;
		y2 = y1 + dy1;
		UG_TextboxCreate(&window_1, &textbox_12, TXB_ID_11, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_11, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_11, "Modules");
		UG_TextboxSetBackColor(&window_1, TXB_ID_11, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_11, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_11, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_13, TXB_ID_12, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_12, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_12, moduleCountText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_12, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_12, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_12, ALIGN_CENTER_RIGHT);
		
	}
	else if(pairingUI_Step == PAIRING_STATE_NOT_FOUND)
	{
				// Create Textbox left button
		UG_TextboxCreate(&window_1, &textbox_3, TXB_ID_2, 0, 144, 43, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_2, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_2, "Scan");
		UG_TextboxSetBackColor(&window_1, TXB_ID_2, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_2, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_2, ALIGN_CENTER_LEFT);
		
		// Create Textbox center button
		UG_TextboxCreate(&window_1, &textbox_4, TXB_ID_3, 44, 144, 83, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_3, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_3, "OK");
		UG_TextboxSetBackColor(&window_1, TXB_ID_3, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_3, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_3, ALIGN_CENTER);

		// Create Textbox rigth button
		UG_TextboxCreate(&window_1, &textbox_5, TXB_ID_4, 84, 144, 127, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_4, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_4, "Exit ");
		UG_TextboxSetBackColor(&window_1, TXB_ID_4, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_4, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_4, ALIGN_CENTER_RIGHT);
		
		UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 15, 72, 123, 82);
		UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_6, "No device found");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER);
	}
	else if(pairingUI_Step == PAIRING_STATE_CONNECTING)
	{
				// Create Textbox left button
		UG_TextboxCreate(&window_1, &textbox_3, TXB_ID_2, 0, 144, 43, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_2, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_2, "");
		UG_TextboxSetBackColor(&window_1, TXB_ID_2, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_2, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_2, ALIGN_CENTER_LEFT);
		
		// Create Textbox center button
		UG_TextboxCreate(&window_1, &textbox_4, TXB_ID_3, 44, 144, 83, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_3, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_3, "");
		UG_TextboxSetBackColor(&window_1, TXB_ID_3, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_3, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_3, ALIGN_CENTER);

		// Create Textbox rigth button
		UG_TextboxCreate(&window_1, &textbox_5, TXB_ID_4, 84, 144, 127, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_4, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_4, "Cancel ");
		UG_TextboxSetBackColor(&window_1, TXB_ID_4, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_4, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_4, ALIGN_CENTER_RIGHT);
		
		UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 15, 52, 123, 62);
		UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_6, "Connecting...");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER);
		
		char snText[20];
		char typeText[20];
		char moduleCountText[20];
		snprintf(snText, sizeof(snText), "0x%08lX", (unsigned long)systemInfo.findDevicefactoryInfo.serial);
		sprintf(typeText, "%c", systemInfo.findDevicefactoryInfo.deviceTypeChar);
		sprintf(moduleCountText, "%02d", systemInfo.findDevicefactoryInfo.numberOfModule);
	
		uint16_t x1, x2, x3, x4, y1, y2, dy1, dy2;
		x1 = 3; x2 = 55; x3 = 57; x4 = 124;	dy1 = 10;	dy2 = 3;		
		y1 = 80;
		y2 = y1 + dy1;
	
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7, "SN");
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER_LEFT);

		UG_TextboxCreate(&window_1, &textbox_9, TXB_ID_8, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_8, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_8, snText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_8, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_8, ALIGN_CENTER_RIGHT);
		
		y1 = y2 + dy2;
		y2 = y1 + dy1;
		UG_TextboxCreate(&window_1, &textbox_10, TXB_ID_9, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_9, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_9, "Type");
		UG_TextboxSetBackColor(&window_1, TXB_ID_9, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_9, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_9, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_11, TXB_ID_10, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_10, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_10, typeText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_10, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_10, ALIGN_CENTER_RIGHT);

		y1 = y2 + dy2;
		y2 = y1 + dy1;
		UG_TextboxCreate(&window_1, &textbox_12, TXB_ID_11, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_11, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_11, "Modules");
		UG_TextboxSetBackColor(&window_1, TXB_ID_11, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_11, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_11, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_13, TXB_ID_12, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_12, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_12, moduleCountText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_12, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_12, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_12, ALIGN_CENTER_RIGHT);
	}
	else if(pairingUI_Step == PAIRING_STATE_SUCCESS)
	{
				// Create Textbox left button
		UG_TextboxCreate(&window_1, &textbox_3, TXB_ID_2, 0, 144, 43, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_2, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_2, "");
		UG_TextboxSetBackColor(&window_1, TXB_ID_2, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_2, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_2, ALIGN_CENTER_LEFT);
		
		// Create Textbox center button
		UG_TextboxCreate(&window_1, &textbox_4, TXB_ID_3, 44, 144, 83, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_3, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_3, "Scan");
		UG_TextboxSetBackColor(&window_1, TXB_ID_3, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_3, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_3, ALIGN_CENTER);

		// Create Textbox rigth button
		UG_TextboxCreate(&window_1, &textbox_5, TXB_ID_4, 84, 144, 127, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_4, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_4, "Exit ");
		UG_TextboxSetBackColor(&window_1, TXB_ID_4, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_4, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_4, ALIGN_CENTER_RIGHT);
		
		UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 15, 52, 123, 62);
		UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_6, "Connected!");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER);
		
		char snText[20];
		char typeText[20];
		char moduleCountText[20];
		char loraIdText[20];
		snprintf(snText, sizeof(snText), "0x%08lX", (unsigned long)systemInfo.findDevicefactoryInfo.serial);
		sprintf(typeText, "%c", systemInfo.findDevicefactoryInfo.deviceTypeChar);
		sprintf(moduleCountText, "%02d", systemInfo.findDevicefactoryInfo.numberOfModule);
		sprintf(loraIdText, "0x%02X", systemInfo.nextNodeId);

		uint16_t x1, x2, x3, x4, y1, y2, dy1, dy2;
		x1 = 3; x2 = 55; x3 = 57; x4 = 124;	dy1 = 10;	dy2 = 3;		
		y1 = 80;
		y2 = y1 + dy1;
	
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7, "SN");
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER_LEFT);

		UG_TextboxCreate(&window_1, &textbox_9, TXB_ID_8, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_8, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_8, snText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_8, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_8, ALIGN_CENTER_RIGHT);
		
		y1 = y2 + dy2;
		y2 = y1 + dy1;
		UG_TextboxCreate(&window_1, &textbox_10, TXB_ID_9, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_9, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_9, "Type");
		UG_TextboxSetBackColor(&window_1, TXB_ID_9, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_9, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_9, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_11, TXB_ID_10, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_10, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_10, typeText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_10, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_10, ALIGN_CENTER_RIGHT);

		y1 = y2 + dy2;
		y2 = y1 + dy1;
		UG_TextboxCreate(&window_1, &textbox_12, TXB_ID_11, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_11, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_11, "Modules");
		UG_TextboxSetBackColor(&window_1, TXB_ID_11, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_11, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_11, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_13, TXB_ID_12, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_12, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_12, moduleCountText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_12, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_12, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_12, ALIGN_CENTER_RIGHT);
		
		y1 = y2 + dy2;
		y2 = y1 + dy1;
		UG_TextboxCreate(&window_1, &textbox_14, TXB_ID_13, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_13, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_13, "Lora ID");
		UG_TextboxSetBackColor(&window_1, TXB_ID_13, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_13, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_13, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_15, TXB_ID_14, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_14, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_14, loraIdText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_14, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_14, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_14, ALIGN_CENTER_RIGHT);
	}
	else if(pairingUI_Step == PAIRING_STATE_FAILED)
	{
				// Create Textbox left button
		UG_TextboxCreate(&window_1, &textbox_3, TXB_ID_2, 0, 144, 43, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_2, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_2, "Retry");
		UG_TextboxSetBackColor(&window_1, TXB_ID_2, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_2, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_2, ALIGN_CENTER_LEFT);
		
		// Create Textbox center button
		UG_TextboxCreate(&window_1, &textbox_4, TXB_ID_3, 44, 144, 83, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_3, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_3, "Scan");
		UG_TextboxSetBackColor(&window_1, TXB_ID_3, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_3, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_3, ALIGN_CENTER);

		// Create Textbox rigth button
		UG_TextboxCreate(&window_1, &textbox_5, TXB_ID_4, 84, 144, 127, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_4, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_4, "Exit ");
		UG_TextboxSetBackColor(&window_1, TXB_ID_4, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_4, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_4, ALIGN_CENTER_RIGHT);
		
		UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 15, 52, 123, 62);
		UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_6, "Connect failed");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER);
		
		char snText[20];
		char typeText[20];
		char moduleCountText[20];
		snprintf(snText, sizeof(snText), "0x%08lX", (unsigned long)systemInfo.findDevicefactoryInfo.serial);
		sprintf(typeText, "%c", systemInfo.findDevicefactoryInfo.deviceTypeChar);
		sprintf(moduleCountText, "%02d", systemInfo.findDevicefactoryInfo.numberOfModule);
	
		uint16_t x1, x2, x3, x4, y1, y2, dy1, dy2;
		x1 = 3; x2 = 55; x3 = 57; x4 = 124;	dy1 = 10;	dy2 = 3;		
		y1 = 80;
		y2 = y1 + dy1;
	
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7, "SN");
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER_LEFT);

		UG_TextboxCreate(&window_1, &textbox_9, TXB_ID_8, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_8, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_8, snText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_8, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_8, ALIGN_CENTER_RIGHT);
		
		y1 = y2 + dy2;
		y2 = y1 + dy1;
		UG_TextboxCreate(&window_1, &textbox_10, TXB_ID_9, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_9, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_9, "Type");
		UG_TextboxSetBackColor(&window_1, TXB_ID_9, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_9, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_9, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_11, TXB_ID_10, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_10, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_10, typeText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_10, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_10, ALIGN_CENTER_RIGHT);

		y1 = y2 + dy2;
		y2 = y1 + dy1;
		UG_TextboxCreate(&window_1, &textbox_12, TXB_ID_11, x1, y1, x2, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_11, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_11, "Modules");
		UG_TextboxSetBackColor(&window_1, TXB_ID_11, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_11, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_11, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_13, TXB_ID_12, x3, y1, x4, y2);
		UG_TextboxSetFont(&window_1, TXB_ID_12, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_12, moduleCountText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_12, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_12, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_12, ALIGN_CENTER_RIGHT);
	}

	UG_WindowShow(&window_1);
	UG_Update();
}
void DPS_Render(void)
{
	Screen_StatusBarUpdate();
	UG_Update();
}
Screen_t devicePairingScreen = 
{ 
	0,
	DPS_Init,
	DPS_Render,
	DPS_LeftButtonClickedCallback,
	DPS_RightButtonClickedCallback,
	DPS_CenterButtonClickedCallback,
	DPS_BLeftButtonClickedCallback,
	DPS_BRightButtonClickedCallback,
	DPS_LeftButtonPressedCallback,
	DPS_RightButtonPressedCallback,
	DPS_CenterButtonPressedCallback,
	DPS_BLeftButtonPressedCallback,
	DPS_BRightButtonPressedCallback
};
