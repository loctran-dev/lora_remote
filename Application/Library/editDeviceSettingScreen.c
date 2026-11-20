#include "linkMonitor.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

static uint8_t indexOfEdittingValue;
static uint8_t indexOfSelection;
static UG_TEXTBOX settingDescription;
static UG_TEXTBOX settingHelp;

static void window_1_callback(UG_MESSAGE *msg)
{
}
static void EDSS_AlarmThresholdCursorUpdate(void)
{
	const uint8_t positions[] = {0, 1, 3, 4};
	UG_RealEditSetCursorPosition(&window_1, REDT_ID_0, positions[indexOfEdittingValue]);
	UG_Update();
}

static void EDSS_AdjustAlarmThreshold(uint8_t increment)
{
	const uint16_t weights[] = {1000, 100, 10, 1};
	uint16_t value = (uint16_t)(UG_RealEditGetValue(&window_1, REDT_ID_0) * 100.0f + 0.5f);
	uint16_t weight = weights[indexOfEdittingValue];
	uint8_t digit = (value / weight) % 10;
	uint8_t next = (digit + (increment ? 1 : 9)) % 10;
	value = value - digit * weight + next * weight;
	UG_RealEditSetValue(&window_1, REDT_ID_0, (float)value / 100.0f);
	UG_Update();
}
void EDSS_LeftButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	if (indexOfSettingMenu == 0)
	{
		EDSS_AdjustAlarmThreshold(1);
	}
	else if ((indexOfSettingMenu == 1 || indexOfSettingMenu == 6))
	{
		char* valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
		
		if (strlen(valueText) == 6
			&& valueText[0] >= '0' && valueText[0] <= '9'
			&& valueText[1] >= '0' && valueText[1] <= '9'
			&& valueText[2] >= '0' && valueText[2] <= '9'
			&& valueText[3] >= '0' && valueText[3] <= '9')
		{
			uint8_t a = valueText[0] - '0';
			uint8_t b = valueText[1] - '0';
			uint8_t c = valueText[2] - '0';
			uint8_t d = valueText[3] - '0';
			if (indexOfEdittingValue == 0)
			{
				a = (a + 1)%10;
			}
			else if (indexOfEdittingValue == 1)
			{
				b = (b + 1)%10;
			}
			else if (indexOfEdittingValue == 2)
			{
				c = (c + 1)%10;
			} 
			else if (indexOfEdittingValue == 3)
			{
				d = (d + 1)%10;
			}
			int newValue = a*1000 + b*100 + c*10 + d;
			char newValueText[20];
			sprintf(newValueText, "%04d s", newValue);
			UG_TextboxSetText(&window_1, TXB_ID_7, newValueText);
			UG_Update();
		}
		free(valueText);
	}
	else if (indexOfSettingMenu == 2 || indexOfSettingMenu == 3 || indexOfSettingMenu == 4)
	{
		if (indexOfSelection == 1) 
		{
			UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_DIM_GRAY);
			UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
			UG_Update();
			indexOfSelection = 0;
		}
	}
	else if (indexOfSettingMenu == 5)
	{
		char* valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
		char newValue[20];

		memset(newValue, ' ', sizeof(newValue) - 1);
		memcpy(newValue, valueText, strlen(valueText) < sizeof(newValue) - 1 ? strlen(valueText) : sizeof(newValue) - 1);
		newValue[sizeof(newValue) - 1] = '\0';
		if (indexOfEdittingValue < sizeof(newValue) - 1)
		{
			if (newValue[indexOfEdittingValue] == '9')
			{
				newValue[indexOfEdittingValue] = 'A';
			}
			else if (newValue[indexOfEdittingValue] == 'Z')
			{
				newValue[indexOfEdittingValue] = 'a';
			}
			else if (newValue[indexOfEdittingValue] == 'z')
			{
				newValue[indexOfEdittingValue] = ' ';
			}
			else if (newValue[indexOfEdittingValue] == ' ')
			{
				newValue[indexOfEdittingValue] = '0';
			}
			else if ((newValue[indexOfEdittingValue] >= '0' && newValue[indexOfEdittingValue] < '9') ||
				(newValue[indexOfEdittingValue] >= 'A' && newValue[indexOfEdittingValue] < 'Z') ||
				(newValue[indexOfEdittingValue] >= 'a' && newValue[indexOfEdittingValue] < 'z'))
			{
				newValue[indexOfEdittingValue] ++;
			}
			else 
			{
				newValue[indexOfEdittingValue] = 'A';
			}
		}
		UG_TextboxSetText(&window_1, TXB_ID_7, newValue);
		UG_Update();
		free(valueText);
	}
}
void EDSS_LeftButtonPressedCallback(void)
{
}
void EDSS_RightButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	
	if (indexOfSettingMenu == 0)
	{
		EDSS_AdjustAlarmThreshold(0);
	}
	else if ((indexOfSettingMenu == 1 || indexOfSettingMenu == 6))
	{
		char* valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
		
		if (strlen(valueText) == 6
			&& valueText[0] >= '0' && valueText[0] <= '9'
			&& valueText[1] >= '0' && valueText[1] <= '9'
			&& valueText[2] >= '0' && valueText[2] <= '9'
			&& valueText[3] >= '0' && valueText[3] <= '9')
		{
			uint8_t a = valueText[0] - '0';
			uint8_t b = valueText[1] - '0';
			uint8_t c = valueText[2] - '0';
			uint8_t d = valueText[3] - '0';
			if (indexOfEdittingValue == 0)
			{
				a = (a + 9)%10;
			}
			else if (indexOfEdittingValue == 1)
			{
				b = (b + 9)%10;
			}
			else if (indexOfEdittingValue == 2)
			{
				c = (c + 9)%10;
			} 
			else if (indexOfEdittingValue == 3)
			{
				d = (d + 9)%10;
			}
			int newValue = a*1000 + b*100 + c*10 + d;
			char newValueText[20];
			sprintf(newValueText, "%04d s", newValue);
			UG_TextboxSetText(&window_1, TXB_ID_7, newValueText);
			UG_Update();
		}
		free(valueText);
	}
	else if (indexOfSettingMenu == 2 || indexOfSettingMenu == 3 || indexOfSettingMenu == 4)
	{
		if (indexOfSelection == 0) 
		{
			UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
			UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_DIM_GRAY);
			indexOfSelection = 1;
		}
	}
	else if (indexOfSettingMenu == 5)
	{
		char* valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
		char newValue[20];

		memset(newValue, ' ', sizeof(newValue) - 1);
		memcpy(newValue, valueText, strlen(valueText) < sizeof(newValue) - 1 ? strlen(valueText) : sizeof(newValue) - 1);
		newValue[sizeof(newValue) - 1] = '\0';
		if (indexOfEdittingValue < sizeof(newValue) - 1)
		{
			if (newValue[indexOfEdittingValue] == '0')
			{
				newValue[indexOfEdittingValue] = ' ';
			}
			else if (newValue[indexOfEdittingValue] == ' ')
			{
				newValue[indexOfEdittingValue] = 'z';
			}
			else if (newValue[indexOfEdittingValue] == 'a')
			{
				newValue[indexOfEdittingValue] = 'Z';
			}
			else if (newValue[indexOfEdittingValue] == 'A')
			{
				newValue[indexOfEdittingValue] = '9';
			}
			else if ((newValue[indexOfEdittingValue] > '0' && newValue[indexOfEdittingValue] <= '9') ||
				(newValue[indexOfEdittingValue] > 'A' && newValue[indexOfEdittingValue] <= 'Z') ||
				(newValue[indexOfEdittingValue] > 'a' && newValue[indexOfEdittingValue] <= 'z'))
			{
				newValue[indexOfEdittingValue] --;
			}
			else 
			{
				newValue[indexOfEdittingValue] = 'z';
			}
		}
		UG_TextboxSetText(&window_1, TXB_ID_7, newValue);
		UG_Update();
		free(valueText);
	}
}
void EDSS_RightButtonPressedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	
	UG_WindowHide(&window_1);
  	UG_WindowDelete(&window_1);
	currentScreen = &deviceSettingScreen;
	currentScreen->init();
	currentScreen->render();
}
void EDSS_CenterButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	
	if (indexOfSettingMenu == 0 || (indexOfSettingMenu == 1 || indexOfSettingMenu == 6))
	{
		indexOfEdittingValue = (indexOfEdittingValue + 1)%4;
		if (indexOfSettingMenu == 0) EDSS_AlarmThresholdCursorUpdate();
	}
	else if (indexOfSettingMenu == 5)
	{
		indexOfEdittingValue = (indexOfEdittingValue + 1)%19;
	}
	else if (indexOfSettingMenu == 2)
	{
		MotorData_t* p_module = &deviceBuff[systemInfo.currentDeviceId].motorData[systemInfo.currentModuleId];
		p_module->isLock = (indexOfSelection + 1)%2;
	}
	else if (indexOfSettingMenu == 3)
	{
		MotorData_t* p_module = &deviceBuff[systemInfo.currentDeviceId].motorData[systemInfo.currentModuleId];
		p_module->isHide = (indexOfSelection + 1)%2;
	}
	else if (indexOfSettingMenu == 4)
	{
		deviceBuff[systemInfo.currentDeviceId].isDelete = (indexOfSelection + 1)%2;
	}	
	
	if (indexOfSettingMenu == 2 || indexOfSettingMenu == 3)
	{
		Screen_SaveDeviceFlags(systemInfo.currentDeviceId);
		UG_WindowHide(&window_1);
		UG_WindowDelete(&window_1);
		currentScreen = indexOfSettingMenu == 3 &&
			deviceBuff[systemInfo.currentDeviceId].motorData[0].isHide ?
			&deviceControlScreen : &deviceSettingScreen;
		currentScreen->init();
		currentScreen->render();
	}
	else if (indexOfSettingMenu == 4)
	{
		Screen_SaveDeviceSettings(systemInfo.currentDeviceId);
		UG_WindowHide(&window_1);
		UG_WindowDelete(&window_1);
		currentScreen = &deviceControlScreen;
		currentScreen->init();
		currentScreen->render();
	}
}
#include "commandHandlers.h"

void EDSS_CenterButtonPressedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	if (indexOfSettingMenu == 0)
	{
		MotorData_t* module = &deviceBuff[systemInfo.currentDeviceId].motorData[systemInfo.currentModuleId];
		module->alarmThreshold = UG_RealEditGetValue(&window_1, REDT_ID_0);
	}
	else if ((indexOfSettingMenu == 1 || indexOfSettingMenu == 6))
	{
		char* valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
		
		if (strlen(valueText) == 6
			&& valueText[0] >= '0' && valueText[0] <= '9'
			&& valueText[1] >= '0' && valueText[1] <= '9'
			&& valueText[2] >= '0' && valueText[2] <= '9'
			&& valueText[3] >= '0' && valueText[3] <= '9')
		{
			uint8_t a = valueText[0] - '0';
			uint8_t b = valueText[1] - '0';
			uint8_t c = valueText[2] - '0';
			uint8_t d = valueText[3] - '0';
			int newValue = a*1000 + b*100 + c*10 + d;
			
			MotorData_t* p_module = &deviceBuff[systemInfo.currentDeviceId].motorData[systemInfo.currentModuleId];
			if (indexOfSettingMenu == 6) Link_SetAutoStatus(systemInfo.currentDeviceId, newValue);
			else p_module->alarmDelayTime = newValue;
		}
		free(valueText);
	}
	else if (indexOfSettingMenu == 2)
	{
		MotorData_t* p_module = &deviceBuff[systemInfo.currentDeviceId].motorData[systemInfo.currentModuleId];
		p_module->isLock = (indexOfSelection + 1)%2;
	}
	else if (indexOfSettingMenu == 3)
	{
		MotorData_t* p_module = &deviceBuff[systemInfo.currentDeviceId].motorData[systemInfo.currentModuleId];
		p_module->isHide = (indexOfSelection + 1)%2;
	}
	else if (indexOfSettingMenu == 4)
	{
		deviceBuff[systemInfo.currentDeviceId].isDelete = (indexOfSelection + 1)%2;
	}	
	else if (indexOfSettingMenu == 5)
	{
		char* valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
		char testingText[20];
		strncpy(testingText, valueText, sizeof(testingText) - 1);
		testingText[sizeof(testingText) - 1] = '\0';
		
		for (int end = strlen(testingText); end > 0 && testingText[end - 1] == ' '; end--)
			testingText[end - 1] = '\0';
		free(valueText);
		MotorData_t* p_module = &deviceBuff[systemInfo.currentDeviceId].motorData[systemInfo.currentModuleId];
		strncpy(p_module->name, testingText, sizeof(testingText)); 
	}
    if (indexOfSettingMenu == 0 || (indexOfSettingMenu == 1 || indexOfSettingMenu == 6))
    {
        Screen_SaveDeviceSettings(systemInfo.currentDeviceId);
        if (!CmdHandler_SendCurrentModuleConfig())
            Main_SetBuzzerMode(BUZZER_LONG_BEEP);
    }
    if (indexOfSettingMenu == 2 || indexOfSettingMenu == 3)
        Screen_SaveDeviceFlags(systemInfo.currentDeviceId);
    if (indexOfSettingMenu == 4 || indexOfSettingMenu == 5)
        Screen_SaveDeviceSettings(systemInfo.currentDeviceId);
	if (indexOfSettingMenu != 4)
	{
		UG_WindowHide(&window_1);
		UG_WindowDelete(&window_1);
		currentScreen = indexOfSettingMenu == 3 &&
			deviceBuff[systemInfo.currentDeviceId].motorData[0].isHide ?
			&deviceControlScreen : &deviceSettingScreen;
		currentScreen->init();
		currentScreen->render();
	}
	else 
	{
		UG_WindowHide(&window_1);
		UG_WindowDelete(&window_1);
		currentScreen = &deviceControlScreen;
		currentScreen->init();
		currentScreen->render();
	}
}
void EDSS_BLeftButtonClickedCallback(void)
{
}
void EDSS_BLeftButtonPressedCallback(void)
{
}
void EDSS_BRightButtonClickedCallback(void)
{
}
void EDSS_BRightButtonPressedCallback(void)
{
}
void EDSS_Init(void)
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
	
	DeviceInfo_t device = deviceBuff[systemInfo.currentDeviceId];
	MotorData_t module = device.motorData[systemInfo.currentModuleId];
	
	char title[40];
	snprintf(title, sizeof(title), " %c%u%u.%.19s-Set.", device.factoryInfo.deviceTypeChar,device.deviceIndex + 1, module.moduleIndex + 1, module.name);
	
	UG_TextboxCreate(&window_1, &textbox_6, TXB_ID_5, 0, 20, 127, 37);
	UG_TextboxSetFont(&window_1, TXB_ID_5, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_5, title);
	UG_TextboxSetBackColor(&window_1, TXB_ID_5, C_DARK_SLATE_BLUE);
	UG_TextboxSetForeColor(&window_1, TXB_ID_5, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_5, ALIGN_CENTER_LEFT);
	
	if (indexOfSettingMenu == 0) 
	{
		// Create Textbox left button
		UG_TextboxCreate(&window_1, &textbox_3, TXB_ID_2, 0, 144, 43, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_2, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_2, " Inc");
		UG_TextboxSetBackColor(&window_1, TXB_ID_2, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_2, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_2, ALIGN_CENTER_LEFT);
		
		// Create Textbox center button
		UG_TextboxCreate(&window_1, &textbox_4, TXB_ID_3, 44, 144, 83, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_3, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_3, "Next");
		UG_TextboxSetBackColor(&window_1, TXB_ID_3, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_3, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_3, ALIGN_CENTER);

		// Create Textbox rigth button
		UG_TextboxCreate(&window_1, &textbox_5, TXB_ID_4, 84, 144, 127, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_4, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_4, "Dec ");
		UG_TextboxSetBackColor(&window_1, TXB_ID_4, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_4, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_4, ALIGN_CENTER_RIGHT);
	
		UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 15, 42, LCD_WIDTH - 1, 52);
		UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_6, "Low-current alarm");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_LEFT);
		
		/*Use Real Edit*/
		UG_RealEditCreate(&window_1, &real_edit_1, REDT_ID_0, 28, 57, 97, 73, 5, 2);
		UG_RealEditSetFont(&window_1, REDT_ID_0, DEFAULT_FONT);
		UG_RealEditSetRange(&window_1, REDT_ID_0, 0.0f, 99.99f);
		UG_RealEditSetValue(&window_1, REDT_ID_0, module.alarmThreshold);
		UG_RealEditSetBackColor(&window_1, REDT_ID_0, C_BLACK);
		UG_RealEditSetForeColor(&window_1, REDT_ID_0, C_WHITE);
		UG_RealEditSetCursorBackColor(&window_1, REDT_ID_0, C_YELLOW);
		UG_RealEditSetCursorForeColor(&window_1, REDT_ID_0, C_BLACK);
		UG_RealEditSetAlignment(&window_1, REDT_ID_0, ALIGN_CENTER);
		UG_RealEditSetCharWidth(&window_1, REDT_ID_0, 14);
		indexOfEdittingValue = 0;
		UG_RealEditSetCursorPosition(&window_1, REDT_ID_0, 0);
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, 101, 57, 113, 73);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7, "A");
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER);
		UG_TextboxCreate(&window_1, &textbox_9, TXB_ID_8, 0, 115, 127, 129);
		UG_TextboxSetFont(&window_1, TXB_ID_8, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_8, "Hold: Save / Back");
		UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_8, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_8, ALIGN_CENTER);
	}
	else if((indexOfSettingMenu == 1 || indexOfSettingMenu == 6))
	{
		// Create Textbox left button
		UG_TextboxCreate(&window_1, &textbox_3, TXB_ID_2, 0, 144, 43, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_2, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_2, " Inc");
		UG_TextboxSetBackColor(&window_1, TXB_ID_2, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_2, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_2, ALIGN_CENTER_LEFT);
		
		// Create Textbox center button
		UG_TextboxCreate(&window_1, &textbox_4, TXB_ID_3, 44, 144, 83, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_3, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_3, "Next");
		UG_TextboxSetBackColor(&window_1, TXB_ID_3, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_3, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_3, ALIGN_CENTER);

		// Create Textbox rigth button
		UG_TextboxCreate(&window_1, &textbox_5, TXB_ID_4, 84, 144, 127, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_4, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_4, "Dec ");
		UG_TextboxSetBackColor(&window_1, TXB_ID_4, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_4, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_4, ALIGN_CENTER_RIGHT);
		
		UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 15, 42, LCD_WIDTH - 1, 52);
		UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_6, indexOfSettingMenu == 6 ? "Auto status interval" : "Alarm delay");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_LEFT);
		
		char alarmDelayTimeText[20];
		sprintf(alarmDelayTimeText, "%04d s", indexOfSettingMenu == 6 ? Link_GetAutoStatus(systemInfo.currentDeviceId) : module.alarmDelayTime);
		
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, 15, 57, 120, 67);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7,alarmDelayTimeText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER);
		
		indexOfEdittingValue = 0;
	}
	else if(indexOfSettingMenu == 2)
	{
		// Create Textbox left button
		UG_TextboxCreate(&window_1, &textbox_3, TXB_ID_2, 0, 144, 43, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_2, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_2, "Up");
		UG_TextboxSetBackColor(&window_1, TXB_ID_2, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_2, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_2, ALIGN_CENTER_LEFT);
		
		// Create Textbox center button
		UG_TextboxCreate(&window_1, &textbox_4, TXB_ID_3, 44, 144, 83, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_3, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_3, "Select");
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
		
		UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 15, 42, LCD_WIDTH - 1, 52);
		UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_6, "Lock");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, 15, 57, 120, 67);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7,"Yes");
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER);
		
		UG_TextboxCreate(&window_1, &textbox_9, TXB_ID_8, 15, 70, 120, 80);
		UG_TextboxSetFont(&window_1, TXB_ID_8, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_8,"No");
		UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_8, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_8, ALIGN_CENTER);
		
		if (module.isLock == 1) 
		{
			UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_DIM_GRAY);
			indexOfSelection = 0;
		}
		else 
		{
			UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_DIM_GRAY);
			indexOfSelection = 1;
		}
	}
	else if(indexOfSettingMenu == 3)
	{
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
		UG_TextboxSetText(&window_1, TXB_ID_3, "Select");
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
		
		UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 15, 42, LCD_WIDTH - 1, 52);
		UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_6, "Hide");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, 15, 57, 120, 67);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7,"Yes");
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER);
		
		UG_TextboxCreate(&window_1, &textbox_9, TXB_ID_8, 15, 70, 120, 80);
		UG_TextboxSetFont(&window_1, TXB_ID_8, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_8,"No");
		UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_8, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_8, ALIGN_CENTER);
		
		if (module.isHide == 1) 
		{
			UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_DIM_GRAY);
			indexOfSelection = 0;
		}
		else 
		{
			UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_DIM_GRAY);
			indexOfSelection = 1;
		}
	}
	else if(indexOfSettingMenu == 4)
	{
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
		UG_TextboxSetText(&window_1, TXB_ID_3, "Select");
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
		
		UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 15, 42, LCD_WIDTH - 1, 52);
		UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_6, "Delete");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, 15, 57, 120, 67);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7,"Yes");
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER);
		
		UG_TextboxCreate(&window_1, &textbox_9, TXB_ID_8, 15, 70, 120, 80);
		UG_TextboxSetFont(&window_1, TXB_ID_8, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_8,"No");
		UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_8, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_8, ALIGN_CENTER);
		
		UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_DIM_GRAY);
		indexOfSelection = 1;
	}
	else if(indexOfSettingMenu == 5)
	{
		// Create Textbox left button
		UG_TextboxCreate(&window_1, &textbox_3, TXB_ID_2, 0, 144, 43, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_2, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_2, " Inc");
		UG_TextboxSetBackColor(&window_1, TXB_ID_2, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_2, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_2, ALIGN_CENTER_LEFT);
		
		// Create Textbox center button
		UG_TextboxCreate(&window_1, &textbox_4, TXB_ID_3, 44, 144, 83, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_3, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_3, "Next");
		UG_TextboxSetBackColor(&window_1, TXB_ID_3, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_3, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_3, ALIGN_CENTER);

		// Create Textbox rigth button
		UG_TextboxCreate(&window_1, &textbox_5, TXB_ID_4, 84, 144, 127, 159);
		UG_TextboxSetFont(&window_1, TXB_ID_4, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_4, "Dec ");
		UG_TextboxSetBackColor(&window_1, TXB_ID_4, C_RED);
		UG_TextboxSetForeColor(&window_1, TXB_ID_4, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_4, ALIGN_CENTER_RIGHT);
		
		UG_TextboxCreate(&window_1, &textbox_7, TXB_ID_6, 15, 42, LCD_WIDTH - 1, 52);
		UG_TextboxSetFont(&window_1, TXB_ID_6, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_6, "Device name");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, 15, 57, 120, 67);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7,module.name);
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER);
		
		indexOfEdittingValue = 0;
	}
	
	if ((indexOfSettingMenu == 1 || indexOfSettingMenu == 6) || indexOfSettingMenu == 5)
	{
		Screen_EditCharactersCreate((indexOfSettingMenu == 1 || indexOfSettingMenu == 6) ? 6 : 19);
		Screen_EditCharactersUpdate(indexOfEdittingValue);
	}
	const char* descriptions[] = {
		"I alarm threshold (A)\nLow current limit",
		"Low current duration\nbefore raising alarm",
		"Block remote ON/OFF\nRemote commands only",
		"Hide from control UI\nUse manager to show",
		"Unpair on this remote\nHistory is retained",
		"Device display name\nEdit one letter/box",
        "Status interval (s)\n0 = status disabled"
	};
	UG_TextboxCreate(&window_1, &settingDescription, 60, 0, 100, 127, 120);
	UG_TextboxSetFont(&window_1, 60, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, 60, (char*)descriptions[indexOfSettingMenu]);
	UG_TextboxSetBackColor(&window_1, 60, C_BLACK);
	UG_TextboxSetForeColor(&window_1, 60, C_WHITE);
	UG_TextboxSetAlignment(&window_1, 60, ALIGN_CENTER_LEFT);
	UG_TextboxSetScrollEnabled(&window_1, 60, 0);
	if (indexOfSettingMenu == 0) UG_TextboxHide(&window_1, TXB_ID_8);
	UG_TextboxCreate(&window_1, &settingHelp, 61, 0, 123, 127, 141);
	UG_TextboxSetFont(&window_1, 61, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, 61, "Hold CENTER: save\nHold RIGHT: cancel");
	UG_TextboxSetBackColor(&window_1, 61, C_BLACK);
	UG_TextboxSetForeColor(&window_1, 61, C_WHITE);
	UG_TextboxSetAlignment(&window_1, 61, ALIGN_CENTER_LEFT);
	UG_TextboxSetScrollEnabled(&window_1, 61, 0);
	UG_WindowShow(&window_1);
	UG_Update();

}
void EDSS_Render(void)
{
	if ((indexOfSettingMenu == 1 || indexOfSettingMenu == 6) || indexOfSettingMenu == 5) Screen_EditCharactersUpdate(indexOfEdittingValue);
	Screen_StatusBarUpdate();
	UG_Update();
}
Screen_t editDeviceSettingScreen = 
{ 
	0,
	EDSS_Init,
	EDSS_Render,
	EDSS_LeftButtonClickedCallback,
	EDSS_RightButtonClickedCallback,
	EDSS_CenterButtonClickedCallback,
	EDSS_BLeftButtonClickedCallback,
	EDSS_BRightButtonClickedCallback,
	EDSS_LeftButtonPressedCallback,
	EDSS_RightButtonPressedCallback,
	EDSS_CenterButtonPressedCallback,
	EDSS_BLeftButtonPressedCallback,
	EDSS_BRightButtonPressedCallback
};
