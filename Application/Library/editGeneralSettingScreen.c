#include "linkMonitor.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "screens.h"
#include "image_data.h"
#include "main.h"
#include "Lora.h"
extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];


static uint8_t indexOfEdittingValue;
static uint8_t indexOfSelection;
static UG_TEXTBOX settingDescription;
static UG_TEXTBOX settingHelp;

static void window_1_callback(UG_MESSAGE *msg)
{
}
void EGSS_LeftButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	const uint8_t dir = 0;
	
	if (indexOfGeneralSettingMenu == 0)
	{
		if (dir == 0) 
		{
			if (indexOfSelection == 1) 
			{
				UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_DIM_GRAY);
				UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
				indexOfSelection = 0;
			}
		}
		else 
		{
			if (indexOfSelection == 0) 
			{
				UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
				UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_DIM_GRAY);
				indexOfSelection = 1;
			}
		}
	}
	else if (indexOfGeneralSettingMenu == 1 || indexOfGeneralSettingMenu == 2 || indexOfGeneralSettingMenu == 6 || indexOfGeneralSettingMenu == 7)
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
				a = (a + 8*dir + 1)%10;
			}
			else if (indexOfEdittingValue == 1)
			{
				b = (b + 8*dir + 1)%10;
			}
			else if (indexOfEdittingValue == 2)
			{
				c = (c + 8*dir + 1)%10;
			} 
			else if (indexOfEdittingValue == 3)
			{
				d = (d + 8*dir + 1)%10;
			}
			int newValue = a*1000 + b*100 + c*10 + d;
			char newValueText[20];
			sprintf(newValueText, "%04d s", newValue);
			UG_TextboxSetText(&window_1, TXB_ID_7, newValueText);
			UG_Update();
		}
		free(valueText);
	}
	else if (indexOfGeneralSettingMenu == 3)
	{
		char* valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
		char newValue[20];

		strncpy(newValue, valueText, sizeof(newValue) - 1);
		newValue[sizeof(newValue) - 1] = '\0';
		if (indexOfEdittingValue < sizeof(newValue) - 1)
		{
			if (dir == 0)
			{
				if (newValue[indexOfEdittingValue] == '9')
				{
					newValue[indexOfEdittingValue] = 'A';
				}
				else if (newValue[indexOfEdittingValue] == 'F')
				{
					newValue[indexOfEdittingValue] = '0';
				}
				else if ((newValue[indexOfEdittingValue] >= '0' && newValue[indexOfEdittingValue] < '9') ||
					(newValue[indexOfEdittingValue] >= 'A' && newValue[indexOfEdittingValue] < 'F'))
				{
					newValue[indexOfEdittingValue] ++;
				}
				else 
				{
					newValue[indexOfEdittingValue] = '0';
				}
			}
			else 
			{
				if (newValue[indexOfEdittingValue] == '0')
				{
					newValue[indexOfEdittingValue] = 'F';
				}
				else if (newValue[indexOfEdittingValue] == 'A')
				{
					newValue[indexOfEdittingValue] = '9';
				}
				else if ((newValue[indexOfEdittingValue] > '0' && newValue[indexOfEdittingValue] <= '9') ||
					(newValue[indexOfEdittingValue] > 'A' && newValue[indexOfEdittingValue] <= 'F'))
				{
					newValue[indexOfEdittingValue] --;
				}
				else 
				{
					newValue[indexOfEdittingValue] = '0';
				}
			}
		}
		UG_TextboxSetText(&window_1, TXB_ID_7, newValue);
		UG_Update();
		free(valueText);
	}
	else if (indexOfGeneralSettingMenu == 4)
	{
		char* valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
		if (strlen(valueText) == 7
			&& valueText[0] >= '0' && valueText[0] <= '9'
			&& valueText[1] >= '0' && valueText[1] <= '9'
			&& valueText[2] >= '0' && valueText[2] <= '9')
		{
			uint8_t a = valueText[0] - '0';
			uint8_t b = valueText[1] - '0';
			uint8_t c = valueText[2] - '0';
			if (indexOfEdittingValue == 0)
			{
				a = (a + 8*dir + 1)%10;
			}
			else if (indexOfEdittingValue == 1)
			{
				b = (b + 8*dir + 1)%10;
			}
			else if (indexOfEdittingValue == 2)
			{
				c = (c + 8*dir + 1)%10;
			} 
			int newValue = a*100 + b*10 + c;
			char newValueText[20];
			sprintf(newValueText, "%03d Mhz", newValue);
			UG_TextboxSetText(&window_1, TXB_ID_7, newValueText);
			UG_Update();
		}
		free(valueText);
	}
	else if (indexOfGeneralSettingMenu == 5)
	{
		if ((dir == 0 && indexOfSelection > 0) || (dir == 1 && indexOfSelection < 5))
		{
			if (dir == 0)
			{
				indexOfSelection--;
			}
			else 
			{
				indexOfSelection++;
			}

			uint8_t txbidMap[] = {TXB_ID_7, TXB_ID_8, TXB_ID_9, TXB_ID_10, TXB_ID_11, TXB_ID_12};
			
			UG_TextboxSetBackColor(&window_1, txbidMap[indexOfSelection], C_DIM_GRAY);
			
			if (txbidMap[indexOfSelection] < TXB_ID_12) 
			{
				UG_TextboxSetBackColor(&window_1, txbidMap[indexOfSelection] + 1, C_BLACK);
			}
			
			if (txbidMap[indexOfSelection] > TXB_ID_7) 
			{
				UG_TextboxSetBackColor(&window_1, txbidMap[indexOfSelection] - 1, C_BLACK);
			}	
			UG_Update();
		}
	}
}
void EGSS_LeftButtonPressedCallback(void)
{
}
void EGSS_RightButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	const uint8_t dir = 1;
	
	if (indexOfGeneralSettingMenu == 0)
	{
		if (dir == 0) 
		{
			if (indexOfSelection == 1) 
			{
				UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_DIM_GRAY);
				UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
				indexOfSelection = 0;
			}
		}
		else 
		{
			if (indexOfSelection == 0) 
			{
				UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
				UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_DIM_GRAY);
				indexOfSelection = 1;
			}
		}
	}
	else if (indexOfGeneralSettingMenu == 1 || indexOfGeneralSettingMenu == 2 || indexOfGeneralSettingMenu == 6 || indexOfGeneralSettingMenu == 7)
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
				a = (a + 8*dir + 1)%10;
			}
			else if (indexOfEdittingValue == 1)
			{
				b = (b + 8*dir + 1)%10;
			}
			else if (indexOfEdittingValue == 2)
			{
				c = (c + 8*dir + 1)%10;
			} 
			else if (indexOfEdittingValue == 3)
			{
				d = (d + 8*dir + 1)%10;
			}
			int newValue = a*1000 + b*100 + c*10 + d;
			char newValueText[20];
			sprintf(newValueText, "%04d s", newValue);
			UG_TextboxSetText(&window_1, TXB_ID_7, newValueText);
			UG_Update();
		}
		free(valueText);
	}
	else if (indexOfGeneralSettingMenu == 3)
	{
		char* valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
		char newValue[20];

		strncpy(newValue, valueText, sizeof(newValue) - 1);
		newValue[sizeof(newValue) - 1] = '\0';
		if (indexOfEdittingValue < sizeof(newValue) - 1)
		{
			if (dir == 0)
			{
				if (newValue[indexOfEdittingValue] == '9')
				{
					newValue[indexOfEdittingValue] = 'A';
				}
				else if (newValue[indexOfEdittingValue] == 'F')
				{
					newValue[indexOfEdittingValue] = '0';
				}
				else if ((newValue[indexOfEdittingValue] >= '0' && newValue[indexOfEdittingValue] < '9') ||
					(newValue[indexOfEdittingValue] >= 'A' && newValue[indexOfEdittingValue] < 'F'))
				{
					newValue[indexOfEdittingValue] ++;
				}
				else 
				{
					newValue[indexOfEdittingValue] = '0';
				}
			}
			else 
			{
				if (newValue[indexOfEdittingValue] == '0')
				{
					newValue[indexOfEdittingValue] = 'F';
				}
				else if (newValue[indexOfEdittingValue] == 'A')
				{
					newValue[indexOfEdittingValue] = '9';
				}
				else if ((newValue[indexOfEdittingValue] > '0' && newValue[indexOfEdittingValue] <= '9') ||
					(newValue[indexOfEdittingValue] > 'A' && newValue[indexOfEdittingValue] <= 'F'))
				{
					newValue[indexOfEdittingValue] --;
				}
				else 
				{
					newValue[indexOfEdittingValue] = '0';
				}
			}
		}
		UG_TextboxSetText(&window_1, TXB_ID_7, newValue);
		UG_Update();
		free(valueText);
	}
	else if (indexOfGeneralSettingMenu == 4)
	{
		char* valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
		if (strlen(valueText) == 7
			&& valueText[0] >= '0' && valueText[0] <= '9'
			&& valueText[1] >= '0' && valueText[1] <= '9'
			&& valueText[2] >= '0' && valueText[2] <= '9')
		{
			uint8_t a = valueText[0] - '0';
			uint8_t b = valueText[1] - '0';
			uint8_t c = valueText[2] - '0';
			if (indexOfEdittingValue == 0)
			{
				a = (a + 8*dir + 1)%10;
			}
			else if (indexOfEdittingValue == 1)
			{
				b = (b + 8*dir + 1)%10;
			}
			else if (indexOfEdittingValue == 2)
			{
				c = (c + 8*dir + 1)%10;
			} 
			int newValue = a*100 + b*10 + c;
			char newValueText[20];
			sprintf(newValueText, "%03d Mhz", newValue);
			UG_TextboxSetText(&window_1, TXB_ID_7, newValueText);
			UG_Update();
		}
		free(valueText);
	}
	else if (indexOfGeneralSettingMenu == 5)
	{
		if ((dir == 0 && indexOfSelection > 0) || (dir == 1 && indexOfSelection < 5))
		{
			if (dir == 0)
			{
				indexOfSelection--;
			}
			else 
			{
				indexOfSelection++;
			}

			uint8_t txbidMap[] = {TXB_ID_7, TXB_ID_8, TXB_ID_9, TXB_ID_10, TXB_ID_11, TXB_ID_12};
			
			UG_TextboxSetBackColor(&window_1, txbidMap[indexOfSelection], C_DIM_GRAY);
			
			if (txbidMap[indexOfSelection] < TXB_ID_12) 
			{
				UG_TextboxSetBackColor(&window_1, txbidMap[indexOfSelection] + 1, C_BLACK);
			}
			
			if (txbidMap[indexOfSelection] > TXB_ID_7) 
			{
				UG_TextboxSetBackColor(&window_1, txbidMap[indexOfSelection] - 1, C_BLACK);
			}	
			UG_Update();
		}
	}
}
void EGSS_RightButtonPressedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	
	UG_WindowHide(&window_1);
  UG_WindowDelete(&window_1);
	currentScreen = &generalSettingScreen;
	currentScreen->init();
	currentScreen->render();
}
void EGSS_CenterButtonClickedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	if (indexOfGeneralSettingMenu == 0)
	{
		systemInfo.sleepMode = (indexOfSelection + 1)%2;
	}
	else if (indexOfGeneralSettingMenu == 1 || indexOfGeneralSettingMenu == 2 || indexOfGeneralSettingMenu == 6 || indexOfGeneralSettingMenu == 7)
	{
		indexOfEdittingValue = (indexOfEdittingValue + 1)%4;
	}
	else if (indexOfGeneralSettingMenu == 3)
	{
		indexOfEdittingValue = (indexOfEdittingValue + 1)%2;
	}
	else if (indexOfGeneralSettingMenu == 4)
	{
		indexOfEdittingValue = (indexOfEdittingValue + 1)%3;
	}
	else if (indexOfGeneralSettingMenu == 5)
	{
		systemInfo.sfConfig = indexOfSelection + SF_7;
	}	
	
	if (indexOfGeneralSettingMenu == 0 || indexOfGeneralSettingMenu == 5)
	{
		Screen_SaveGeneralSettings();
		UG_WindowHide(&window_1);
		UG_WindowDelete(&window_1);
		currentScreen = &generalSettingScreen;
		currentScreen->init();
		currentScreen->render();
	}
}
void EGSS_CenterButtonPressedCallback(void)
{
	Main_SetBuzzerMode(BUZZER_SHORT_BEEP);
	if (indexOfGeneralSettingMenu == 0)
	{
		systemInfo.sleepMode = (indexOfSelection + 1)%2;
	}
	else if (indexOfGeneralSettingMenu == 1)
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
			if (newValue < 1) {
				free(valueText);
				Main_SetBuzzerMode(BUZZER_LONG_BEEP);
				return;
			}
			systemInfo.tUpdate = newValue;
		}
		free(valueText);
	}
	else if (indexOfGeneralSettingMenu == 2)
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
			if (newValue < 1) {
				free(valueText);
				Main_SetBuzzerMode(BUZZER_LONG_BEEP);
				return;
			}
			systemInfo.tSleepUpdate = newValue;
		}
		free(valueText);
	}
	else if (indexOfGeneralSettingMenu == 3)
	{
		char* valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
		char *endPtr;
		long net = strtol(valueText, &endPtr, 16);
		if (*endPtr || net <= 0 || net >= 255) {
			free(valueText);
			Main_SetBuzzerMode(BUZZER_LONG_BEEP);
			return;
		}
		systemInfo.netIdConfig = net;
		free(valueText);
	}
	else if (indexOfGeneralSettingMenu == 4)
	{
		char* valueText = UG_TextboxGetText(&window_1, TXB_ID_7);
		if (strlen(valueText) == 7
			&& valueText[0] >= '0' && valueText[0] <= '9'
			&& valueText[1] >= '0' && valueText[1] <= '9'
			&& valueText[2] >= '0' && valueText[2] <= '9')
		{
			uint8_t a = valueText[0] - '0';
			uint8_t b = valueText[1] - '0';
			uint8_t c = valueText[2] - '0';
			int newValue = a*100 + b*10 + c;
			if (newValue < 137) {
				free(valueText);
				Main_SetBuzzerMode(BUZZER_LONG_BEEP);
				return;
			}
			systemInfo.frequencyConfig = newValue;
		}
		free(valueText);
	}	
	else if (indexOfGeneralSettingMenu == 5)
	{
		systemInfo.sfConfig = indexOfSelection + SF_7;
	}
	else if (indexOfGeneralSettingMenu == 6 || indexOfGeneralSettingMenu == 7)
    {
        char *text = UG_TextboxGetText(&window_1, TXB_ID_7);
        unsigned value = 0;
        uint8_t valid = strlen(text) == 6;
        for (uint8_t i = 0; i < 4 && valid; i++) {
            if (text[i] < '0' || text[i] > '9') valid = 0;
            else value = value * 10 + text[i] - '0';
        }
        free(text);
        if (!valid || !value) { Main_SetBuzzerMode(BUZZER_LONG_BEEP); return; }
        if (indexOfGeneralSettingMenu == 6) Link_SetIdleSeconds(value);
        else Link_SetTimeoutSeconds(value);
    }
	Screen_SaveGeneralSettings();

	UG_WindowHide(&window_1);
	UG_WindowDelete(&window_1);
	currentScreen = &generalSettingScreen;
	currentScreen->init();
	currentScreen->render();
}
void EGSS_BLeftButtonClickedCallback(void)
{
}
void EGSS_BLeftButtonPressedCallback(void)
{
}
void EGSS_BRightButtonClickedCallback(void)
{
}
void EGSS_BRightButtonPressedCallback(void)
{
}
void EGSS_Init(void)
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
	
	// Create Textbox 
	char title[30];
	sprintf(title, "General setting");
	
	UG_TextboxCreate(&window_1, &textbox_6, TXB_ID_5, 0, 20, LCD_WIDTH - 1, 37);
	UG_TextboxSetFont(&window_1, TXB_ID_5, DEFAULT_FONT);
	UG_TextboxSetText(&window_1, TXB_ID_5, title);
	UG_TextboxSetBackColor(&window_1, TXB_ID_5, C_DARK_SLATE_BLUE);
	UG_TextboxSetForeColor(&window_1, TXB_ID_5, C_WHITE);
	UG_TextboxSetAlignment(&window_1, TXB_ID_5, ALIGN_CENTER_LEFT);
	
	if (indexOfGeneralSettingMenu == 0) 
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
		UG_TextboxSetText(&window_1, TXB_ID_6, "Enable sleep mode");
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
		
		if (systemInfo.sleepMode == 1) 
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
	else if(indexOfGeneralSettingMenu == 1 || indexOfGeneralSettingMenu == 6 || indexOfGeneralSettingMenu == 7)
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
		UG_TextboxSetText(&window_1, TXB_ID_6, indexOfGeneralSettingMenu == 6 ? "Sleep after idle" : indexOfGeneralSettingMenu == 7 ? "Link timeout" : "Link scan interval");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_LEFT);
		
		char tbText[20];
		sprintf(tbText, "%04d s", indexOfGeneralSettingMenu == 6 ? Link_GetIdleSeconds() : indexOfGeneralSettingMenu == 7 ? Link_GetTimeoutSeconds() : systemInfo.tUpdate);
		
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, 15, 57, 120, 67);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7,tbText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER);
		
		indexOfEdittingValue = 0;
	}
	else if(indexOfGeneralSettingMenu == 2)
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
		UG_TextboxSetText(&window_1, TXB_ID_6, "Sleep check interval");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_LEFT);
		
		char tbText[20];
		sprintf(tbText, "%04d s", systemInfo.tSleepUpdate);
		
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, 15, 57, 120, 67);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7,tbText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER);
		
		indexOfEdittingValue = 0;
	}
	else if(indexOfGeneralSettingMenu == 3)
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
		UG_TextboxSetText(&window_1, TXB_ID_6, "LoRa network ID");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_LEFT);
		
		char tbText[20];
		sprintf(tbText, "%02X", systemInfo.netIdConfig);
		
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, 15, 57, 120, 67);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7,tbText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER);
		
		indexOfEdittingValue = 0;
	}
	else if(indexOfGeneralSettingMenu == 4)
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
		UG_TextboxSetText(&window_1, TXB_ID_6, "LoRa frequency (MHz)");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_LEFT);
		
		char tbText[20];
		sprintf(tbText, "%03d Mhz", systemInfo.frequencyConfig);
		
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, 15, 57, 120, 67);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7,tbText);
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER);
		
		indexOfEdittingValue = 0;
	}
	else if(indexOfGeneralSettingMenu == 5)
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
		UG_TextboxSetText(&window_1, TXB_ID_6, "LoRa spreading factor");
		UG_TextboxSetBackColor(&window_1, TXB_ID_6, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_6, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_6, ALIGN_CENTER_LEFT);
		
		UG_TextboxCreate(&window_1, &textbox_8, TXB_ID_7, 15, 57, 120, 67);
		UG_TextboxSetFont(&window_1, TXB_ID_7, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_7,"SF7");
		UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_7, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_7, ALIGN_CENTER);
		
		UG_TextboxCreate(&window_1, &textbox_9, TXB_ID_8, 15, 70, 120, 80);
		UG_TextboxSetFont(&window_1, TXB_ID_8, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_8,"SF8");
		UG_TextboxSetBackColor(&window_1, TXB_ID_8, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_8, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_8, ALIGN_CENTER);
		
		UG_TextboxCreate(&window_1, &textbox_10, TXB_ID_9, 15, 83, 120, 93);
		UG_TextboxSetFont(&window_1, TXB_ID_9, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_9,"SF9");
		UG_TextboxSetBackColor(&window_1, TXB_ID_9, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_9, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_9, ALIGN_CENTER);
		
		UG_TextboxCreate(&window_1, &textbox_11, TXB_ID_10, 15, 96, 120, 106);
		UG_TextboxSetFont(&window_1, TXB_ID_10, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_10,"SF10");
		UG_TextboxSetBackColor(&window_1, TXB_ID_10, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_10, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_10, ALIGN_CENTER);
		
		UG_TextboxCreate(&window_1, &textbox_12, TXB_ID_11, 15, 109, 120, 119);
		UG_TextboxSetFont(&window_1, TXB_ID_11, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_11,"SF11");
		UG_TextboxSetBackColor(&window_1, TXB_ID_11, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_11, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_11, ALIGN_CENTER);
		
		UG_TextboxCreate(&window_1, &textbox_13, TXB_ID_12, 15, 122, 120, 132);
		UG_TextboxSetFont(&window_1, TXB_ID_12, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, TXB_ID_12,"SF12");
		UG_TextboxSetBackColor(&window_1, TXB_ID_12, C_BLACK);
		UG_TextboxSetForeColor(&window_1, TXB_ID_12, C_WHITE);
		UG_TextboxSetAlignment(&window_1, TXB_ID_12, ALIGN_CENTER);
		
		uint8_t txb_id_selected = systemInfo.sfConfig - SF_7 + TXB_ID_7;
		if (systemInfo.sfConfig >= SF_7 && systemInfo.sfConfig <= SF_12)
		{
			UG_TextboxSetBackColor(&window_1, txb_id_selected, C_DIM_GRAY);
			indexOfSelection = systemInfo.sfConfig - SF_7;
		}
		else 
		{
			UG_TextboxSetBackColor(&window_1, TXB_ID_7, C_DIM_GRAY);
			indexOfSelection = 0;
		}
	}
	
	if ((indexOfGeneralSettingMenu >= 1 && indexOfGeneralSettingMenu <= 4) || indexOfGeneralSettingMenu >= 6)
	{
		uint8_t counts[] = {0, 6, 6, 2, 7, 0, 6, 6};
		Screen_EditCharactersCreate(counts[indexOfGeneralSettingMenu]);
		Screen_EditCharactersUpdate(indexOfEdittingValue);
	}
	if (indexOfGeneralSettingMenu != 5)
	{
		const char* descriptions[] = {
            "Enable idle sleep\nRTC/LoRa can wake",
            "Local link scan (s)\nNo periodic polling",
            "Sleep RTC wake (s)\nCheck silent devices",
            "LoRa network ID\n01-FE hex",
            "Radio frequency (MHz)\n137-999 in editor",
            "LoRa spreading factor",
            "Idle before sleep (s)\nNo button activity",
            "Silence threshold (s)\nThen verify status"
		};
		UG_TextboxCreate(&window_1, &settingDescription, 60, 0, 100, 127, 120);
		UG_TextboxSetFont(&window_1, 60, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, 60, (char*)descriptions[indexOfGeneralSettingMenu]);
		UG_TextboxSetBackColor(&window_1, 60, C_BLACK);
		UG_TextboxSetForeColor(&window_1, 60, C_WHITE);
		UG_TextboxSetAlignment(&window_1, 60, ALIGN_CENTER_LEFT);
		UG_TextboxSetScrollEnabled(&window_1, 60, 0);
		UG_TextboxCreate(&window_1, &settingHelp, 61, 0, 123, 127, 141);
		UG_TextboxSetFont(&window_1, 61, DEFAULT_FONT);
		UG_TextboxSetText(&window_1, 61, "Hold CENTER: save\nHold RIGHT: cancel");
		UG_TextboxSetBackColor(&window_1, 61, C_BLACK);
		UG_TextboxSetForeColor(&window_1, 61, C_WHITE);
		UG_TextboxSetAlignment(&window_1, 61, ALIGN_CENTER_LEFT);
		UG_TextboxSetScrollEnabled(&window_1, 61, 0);
	}
	UG_WindowShow(&window_1);
	UG_Update();

}
void EGSS_Render(void)
{
	if ((indexOfGeneralSettingMenu >= 1 && indexOfGeneralSettingMenu <= 4) || indexOfGeneralSettingMenu >= 6)
		Screen_EditCharactersUpdate(indexOfEdittingValue);
	if (indexOfGeneralSettingMenu == 5)
		for (uint8_t i = 0; i < 6; i++)
			UG_TextboxSetBackColor(&window_1, TXB_ID_7 + i, i == indexOfSelection ? C_DIM_GRAY : C_BLACK);
	Screen_StatusBarUpdate();
	UG_Update();
}
Screen_t editGeneralSettingScreen = 
{ 
	0,
	EGSS_Init,
	EGSS_Render,
	EGSS_LeftButtonClickedCallback,
	EGSS_RightButtonClickedCallback,
	EGSS_CenterButtonClickedCallback,
	EGSS_BLeftButtonClickedCallback,
	EGSS_BRightButtonClickedCallback,
	EGSS_LeftButtonPressedCallback,
	EGSS_RightButtonPressedCallback,
	EGSS_CenterButtonPressedCallback,
	EGSS_BLeftButtonPressedCallback,
	EGSS_BRightButtonPressedCallback
};
