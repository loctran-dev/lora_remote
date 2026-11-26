/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_cdc_if.c
  * @version        : v2.0_Cube
  * @brief          : Usb device for Virtual Com Port.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc_if.h"

/* USER CODE BEGIN INCLUDE */
#include "lcd.h"
#include <string.h>
#include <stdio.h>
#include "screens.h"
#include "link_monitor.h"
#include "power_sleep.h"
#include "command_handlers.h"
#include "schedule.h"
#include "remote_history.h"
#include "rtc_setting.h"
#include "config_store.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "task.h"
extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];
#include "lora.h"
extern LoRa sLoRaCtrl;

/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
static volatile uint32_t echoLength;
static uint8_t echoTransmitting;
static uint8_t probePending;
static uint8_t responsePrepared;
static uint8_t bootResetPending;
static char buttonCommand[64];
static uint8_t buttonCommandLength;
extern osThreadId_t systemTaskHandle, lcdTaskHandle, loraSendTaskHandle, loraReceiveTaskHandle;
extern osThreadId_t sensorTaskHandle, buzzerTaskHandle;
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief Usb device library.
  * @{
  */

/** @addtogroup USBD_CDC_IF
  * @{
  */

/** @defgroup USBD_CDC_IF_Private_TypesDefinitions USBD_CDC_IF_Private_TypesDefinitions
  * @brief Private types.
  * @{
  */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Defines USBD_CDC_IF_Private_Defines
  * @brief Private defines.
  * @{
  */

/* USER CODE BEGIN PRIVATE_DEFINES */
/* USER CODE END PRIVATE_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Macros USBD_CDC_IF_Private_Macros
  * @brief Private macros.
  * @{
  */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Variables USBD_CDC_IF_Private_Variables
  * @brief Private variables.
  * @{
  */
/* Create buffer for reception and transmission           */
/* It's up to user to redefine and/or remove those define */
/** Received data over USB are stored in this buffer      */
uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];

/** Data to send over USB CDC are stored in this buffer   */
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* USER CODE BEGIN PRIVATE_VARIABLES */

/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Exported_Variables USBD_CDC_IF_Exported_Variables
  * @brief Public variables.
  * @{
  */

extern USBD_HandleTypeDef hUsbDeviceFS;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_FunctionPrototypes USBD_CDC_IF_Private_FunctionPrototypes
  * @brief Private functions declaration.
  * @{
  */

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t* pbuf, uint32_t *Len);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */

/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/**
  * @}
  */

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Initializes the CDC media low layer over the FS USB IP
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Init_FS(void)
{
  /* USER CODE BEGIN 3 */
  /* Set Application Buffers */
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  return (USBD_OK);
  /* USER CODE END 3 */
}

/**
  * @brief  DeInitializes the CDC media low layer
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_DeInit_FS(void)
{
  /* USER CODE BEGIN 4 */
  return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  Manage the CDC class requests
  * @param  cmd: Command code
  * @param  pbuf: Buffer containing command data (request parameters)
  * @param  length: Number of data to be sent (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  /* USER CODE BEGIN 5 */
  switch(cmd)
  {
    case CDC_SEND_ENCAPSULATED_COMMAND:

    break;

    case CDC_GET_ENCAPSULATED_RESPONSE:

    break;

    case CDC_SET_COMM_FEATURE:

    break;

    case CDC_GET_COMM_FEATURE:

    break;

    case CDC_CLEAR_COMM_FEATURE:

    break;

  /*******************************************************************************/
  /* Line Coding Structure                                                       */
  /*-----------------------------------------------------------------------------*/
  /* Offset | Field       | Size | Value  | Description                          */
  /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
  /* 4      | bCharFormat |   1  | Number | Stop bits                            */
  /*                                        0 - 1 Stop bit                       */
  /*                                        1 - 1.5 Stop bits                    */
  /*                                        2 - 2 Stop bits                      */
  /* 5      | bParityType |  1   | Number | Parity                               */
  /*                                        0 - None                             */
  /*                                        1 - Odd                              */
  /*                                        2 - Even                             */
  /*                                        3 - Mark                             */
  /*                                        4 - Space                            */
  /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
  /*******************************************************************************/
    case CDC_SET_LINE_CODING:

    break;

    case CDC_GET_LINE_CODING:

    break;

    case CDC_SET_CONTROL_LINE_STATE:

    break;

    case CDC_SEND_BREAK:

    break;

  default:
    break;
  }

  return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  Data received over USB OUT endpoint are sent over CDC interface
  *         through this function.
  *
  *         @note
  *         This function will issue a NAK packet on any OUT packet received on
  *         USB endpoint until exiting this function. If you exit this function
  *         before transfer is complete on CDC interface (ie. using DMA controller)
  *         it will result in receiving more data while previous ones are still
  *         not sent.
  *
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */
  /* Keep the receive buffer owned until the echo has completed.
     Leaving OUT unarmed applies USB backpressure without dropping packets. */
  echoLength = *Len;
  (void)Buf;
  return (USBD_OK);
  /* USER CODE END 6 */
}

/**
  * @brief  CDC_Transmit_FS
  *         Data to send over USB IN endpoint are sent over CDC interface
  *         through this function.
  *         @note
  *
  *
  * @param  Buf: Buffer of data to be sent
  * @param  Len: Number of data to be sent (in bytes)
  * @retval USBD_OK if all operations are OK else USBD_FAIL or USBD_BUSY
  */
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 7 */
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  if (hcdc == NULL || hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) { return USBD_FAIL; }
  if (hcdc->TxState != 0){
    return USBD_BUSY;
  }
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  /* USER CODE END 7 */
  return result;
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */
static const char *CDC_ScreenName(void)
{
  if (currentScreen == &deviceControlScreen) return "CONTROL";
  if (currentScreen == &historyScreen) return "HISTORY";
  if (currentScreen == &deviceMonitorScreen) return "MONITOR";
  if (currentScreen == &deviceSettingScreen) return "DEVICE_SETTING";
  if (currentScreen == &editDeviceSettingScreen) return "EDIT_DEVICE_SETTING";
  if (currentScreen == &mainMenuScreen) return "MENU";
  if (currentScreen == &deviceManagerScreen) return "DEVICE_MANAGER";
  if (currentScreen == &generalSettingScreen) return "GENERAL_SETTING";
  if (currentScreen == &editGeneralSettingScreen) return "EDIT_GENERAL_SETTING";
  if (currentScreen == &dateTimeScreen) return "DATE_TIME";
  if (currentScreen == &devicePairingScreen) return "PAIRING";
  return "UNKNOWN";
}

/* Runs in systemTask, the same context as physical button dispatch. */
static void CDC_ButtonCommand(void)
{
  char button[12], action[12], extra;
  void (*callback)(void) = NULL;
  uint8_t isLong;
  if (sscanf(buttonCommand, "BTN %11s %11s %c", button, action, &extra) != 2 ||
      (strcmp(action, "CLICK") != 0 && strcmp(action, "LONG") != 0))
  {
    echoLength = sprintf((char*)UserRxBufferFS, "ERR BTN syntax: BTN LEFT|RIGHT|CENTER|ON|OFF CLICK|LONG\n");
    return;
  }
  isLong = strcmp(action, "LONG") == 0;
  if (currentScreen != NULL)
  {
    if (strcmp(button, "LEFT") == 0)
      callback = isLong ? currentScreen->leftButtonPressedCallback : currentScreen->leftButtonClickedCallback;
    else if (strcmp(button, "RIGHT") == 0)
      callback = isLong ? currentScreen->rightButtonPressedCallback : currentScreen->rightButtonClickedCallback;
    else if (strcmp(button, "CENTER") == 0)
      callback = isLong ? currentScreen->centerButtonPressedCallback : currentScreen->centerButtonClickedCallback;
    else if (strcmp(button, "ON") == 0)
      callback = isLong ? currentScreen->bleftButtonPressedCallback : currentScreen->bleftButtonClickedCallback;
    else if (strcmp(button, "OFF") == 0)
      callback = isLong ? currentScreen->brightButtonPressedCallback : currentScreen->brightButtonClickedCallback;
  }
  if (callback == NULL)
  {
    echoLength = sprintf((char*)UserRxBufferFS, "ERR BTN unknown button or unavailable callback\n");
    return;
  }
  systemInfo.lastActionTick = xTaskGetTickCount();
  Power_UserActivity();
  callback();
  if (currentScreen == &deviceManagerScreen) {
    uint8_t id = ScrnDvm_SelectedDevice();
    if (id < MAXIMUM_DEVICE) {
      echoLength = sprintf((char*)UserRxBufferFS,
        "OK BTN %s %s SCREEN=DEVICE_MANAGER DEV=%u HIDE=%u LOCK=%u\n",
        button, action, id + 1, Config_DeviceHidden(id),
        Config_Motor(id, 0) != NULL && Config_Motor(id, 0)->locked);
      return;
    }
  }
  echoLength = sprintf((char*)UserRxBufferFS, "OK BTN %s %s SCREEN=%s\n", button, action, CDC_ScreenName());
}

void CDC_ProcessEcho_FS(void)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED || hcdc == NULL)
  {
    echoLength = 0;
    echoTransmitting = 0;
    buttonCommandLength = 0;
    bootResetPending = 0;
    probePending = 0;
    responsePrepared = 0;
    return;
  }
  if (echoTransmitting)
  {
    if (hcdc->TxState == 0)
    {
      echoTransmitting = 0;
      echoLength = 0;
      responsePrepared = 0;
      if (bootResetPending)
      {
        __HAL_RCC_PWR_CLK_ENABLE();
        __HAL_RCC_BKP_CLK_ENABLE();
        HAL_PWR_EnableBkUpAccess();
        BKP->DR10 = 0xB007u;
        NVIC_SystemReset();
      }
      USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
      USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    }
  }
  else if (echoLength != 0)
  {
    if (!responsePrepared && !probePending && echoLength == 11 &&
        memcmp(UserRxBufferFS, "BOOTLOADER\n", 11) == 0)
    {
      bootResetPending = 1;
      echoLength = sprintf((char*)UserRxBufferFS, "OK BOOTLOADER\n");
      responsePrepared = 1;
    }
    if (!responsePrepared && !probePending &&
        ((echoLength==5 && UserRxBufferFS[4]=='\n') || (echoLength>=7 && UserRxBufferFS[4]==' ')) &&
        memcmp(UserRxBufferFS,"TIME",4)==0)
    {
      uint8_t result=eRtc_SetResult_Ok;
      uint32_t stamp=0;
      if (!(echoLength==5 && UserRxBufferFS[4]=='\n')) {
        if (!Rtc_Parse(UserRxBufferFS,echoLength,&stamp)) result=eRtc_SetResult_Range;
        else result=Rtc_Set(stamp);
      }
      if (result==eRtc_SetResult_Ok) {
        stamp=Rtc_Read();
        echoLength=snprintf((char*)UserRxBufferFS,APP_RX_DATA_SIZE,
          "TIME TS=%lu DATE=%04u-%02u-%02u CLOCK=%02u:%02u:%02u\n",(unsigned long)stamp,
          2000u+systemInfo.rtcDate.Year,systemInfo.rtcDate.Month,systemInfo.rtcDate.Date,
          systemInfo.rtcTime.Hours,systemInfo.rtcTime.Minutes,systemInfo.rtcTime.Seconds);
      } else echoLength=snprintf((char*)UserRxBufferFS,APP_RX_DATA_SIZE,"ERR TIME %s\n",
          result==eRtc_SetResult_Busy?"ACTIVE_TIMER":result==eRtc_SetResult_Io?"RTC_WRITE":"SYNTAX_OR_RANGE");
      responsePrepared=1;
    }
    if (!responsePrepared && !probePending && echoLength == 8 &&
        memcmp(UserRxBufferFS, "BATTERY\n", 8) == 0)
    {
      echoLength = sprintf((char*)UserRxBufferFS,
        "BATTERY ADC=%lu VBAT_MV=%lu PCT=%u CHARGING=%u FULL=%u VALID=%u\n",
        (unsigned long)systemInfo.batteryAdc,
        (unsigned long)(systemInfo.batteryVoltage * 1000.0f + 0.5f),
        systemInfo.batteryPercent, systemInfo.charging, systemInfo.batteryFull,
        systemInfo.batteryVoltage > 0);
      responsePrepared = 1;
    }
    if (!responsePrepared && !probePending && echoLength == 7 &&
        memcmp(UserRxBufferFS, "CONFIG\n", 7) == 0)
    {
      echoLength = sprintf((char*)UserRxBufferFS,
        "CONFIG NET=%u FREQ=%d SF=%u SLEEP=%u UPDATE=%lu SLEEP_UPDATE=%lu ACTIVE_NET=%u ACTIVE_FREQ=%u ACTIVE_SF=%u IDLE=%u LOST=%u AUTO=%u\n",
        storageData.netId, storageData.freqMhz, storageData.spreadFactor,
        storageData.sleepEnabled, (unsigned long)storageData.scanPeriod,
        (unsigned long)storageData.sleepScanPeriod, systemInfo.netId,
        sLoRaCtrl.frequency, sLoRaCtrl.spredingFactor, Link_GetIdleSeconds(),
        Link_GetTimeoutSeconds(), Link_GetAutoStatus(systemInfo.curDevice));
      responsePrepared = 1;
    }
    if (!responsePrepared && !probePending && echoLength == 8 && memcmp(UserRxBufferFS,"HISTORY\n",8)==0)
    {
      unsigned count=0;
      for (uint16_t i=0;i<HJ_SLOTS;i++) if (HJ_Get(i)) count++;
      echoLength=snprintf((char*)UserRxBufferFS,APP_RX_DATA_SIZE,
        "HISTORY COUNT=%u STORED=%lu DUP=%lu ACK=%lu REJECT=%lu ERR=%lu MODE=%u\n",count,
        (unsigned long)remoteHistoryStored,(unsigned long)remoteHistoryDuplicates,
        (unsigned long)remoteHistoryAcked,(unsigned long)remoteHistoryRejected,
        (unsigned long)historyStoreErrors,historyScreenMode);
      responsePrepared=1;
    }
    if (!responsePrepared && !probePending && echoLength >= 7 && echoLength <= 12 &&
        memcmp(UserRxBufferFS,"EVENT ",6)==0)
    {
      unsigned slot=HJ_SLOTS;
      sscanf((char*)UserRxBufferFS+6,"%u",&slot);
      const HistoryRecord *r=slot<HJ_SLOTS?HJ_Get(slot):0;
      if (!r) echoLength=sprintf((char*)UserRxBufferFS,"ERR EVENT slot empty\n");
      else echoLength=snprintf((char*)UserRxBufferFS,APP_RX_DATA_SIZE,
        "EVENT SLOT=%u OWNER=%u SERIAL=%lu ID=%lu TIME=%lu ACTION=%u SOURCE=%u BEFORE=%u AFTER=%u RESULT=%u ERROR=%u FLAGS=%u\n",
        slot,r->owner,(unsigned long)r->event.serial,(unsigned long)r->event.id,
        (unsigned long)r->event.timestamp,r->event.action,r->event.source,r->event.before,r->event.after,
        r->event.result,r->event.error,r->event.flags);
      responsePrepared=1;
    }
    if (!responsePrepared && !probePending && echoLength == 6 &&
        memcmp(UserRxBufferFS, "TIMER\n", 6) == 0)
    {
      const MotorData_t *motor = ScrnCommon_CurrentMotorState();
      Schedule_RequestState_t scheduleState;
      Schedule_GetRequestState(&scheduleState);
      echoLength = snprintf((char*)UserRxBufferFS, APP_RX_DATA_SIZE,
        "TIMER NOW=%lu PWR=%u DEADLINE=%lu STATE=%u NODE=%u ID=%u TRY=%u TX_OK=%u SYNC=%lu TARGET=%lu\n",
        (unsigned long)systemInfo.currentTime, motor->pwrState, (unsigned long)motor->autoOffTime,
        scheduleState.state, scheduleState.node, scheduleState.transaction, scheduleState.attempts,
        scheduleState.txOk, (unsigned long)scheduleState.syncTime, (unsigned long)scheduleState.deadline);
      responsePrepared = 1;
    }
    if (!responsePrepared && !probePending && echoLength == 5 &&
        memcmp(UserRxBufferFS, "DIAG\n", 5) == 0)
    {
      echoLength = sprintf((char*)UserRxBufferFS,
        "DIAG HEAP=%lu MIN_HEAP=%lu STACK_SYS=%lu STACK_LCD=%lu STACK_TX=%lu STACK_RX=%lu STACK_KEY=%lu STACK_BEEP=%lu LCD_ERR=%lu LCD_REC=%lu LCD_FAULT=%u RF_SPI_ERR=%lu\n",
        (unsigned long)xPortGetFreeHeapSize(), (unsigned long)xPortGetMinimumEverFreeHeapSize(),
        (unsigned long)uxTaskGetStackHighWaterMark(systemTaskHandle) * sizeof(StackType_t),
        (unsigned long)uxTaskGetStackHighWaterMark(lcdTaskHandle) * sizeof(StackType_t),
        (unsigned long)uxTaskGetStackHighWaterMark(loraSendTaskHandle) * sizeof(StackType_t),
        (unsigned long)uxTaskGetStackHighWaterMark(loraReceiveTaskHandle) * sizeof(StackType_t),
        (unsigned long)uxTaskGetStackHighWaterMark(sensorTaskHandle) * sizeof(StackType_t),
        (unsigned long)uxTaskGetStackHighWaterMark(buzzerTaskHandle) * sizeof(StackType_t),
        (unsigned long)lcdTransferErrors, (unsigned long)lcdRecoveries, LCD_HasFault(),
        (unsigned long)loraSpiErrors);
      responsePrepared = 1;
    }
    if (!responsePrepared && !probePending && echoLength >= 5 && echoLength <= 8 &&
        memcmp(UserRxBufferFS, "LINK", 4) == 0)
    {
      unsigned id = systemInfo.curDevice;
      if (echoLength > 5) {
        unsigned number = 0;
        for (unsigned i = 5; i < echoLength && UserRxBufferFS[i] != '\n'; i++) {
          if (UserRxBufferFS[i] < '0' || UserRxBufferFS[i] > '9') { number = 255; break; }
          number = number * 10 + UserRxBufferFS[i] - '0';
        }
        id = number ? number - 1 : MAXIMUM_DEVICE;
      }
      if (id >= MAXIMUM_DEVICE) echoLength = sprintf((char*)UserRxBufferFS, "ERR LINK device 1..20\n");
      else echoLength = sprintf((char*)UserRxBufferFS,
        "LINK DEV=%u NOW=%lu RX_AGE=%lu STATUS_AGE=%lu ERROR=%u PROBE=%d TRY=%u STATE=%u DEADLINE=%lu REQUESTS=%lu OK=%lu FAIL=%lu STATUS=%lu MODE=%u STOP=%lu RTC=%lu RF=%lu KEY=%lu RX_PENDING=%u\n",
        id + 1, (unsigned long)Link_Now(), (unsigned long)(Link_Now() - linkDevices[id].lastRx),
        (unsigned long)(Link_Now() - linkDevices[id].lastStatus), deviceBuff[id].linkLost,
        linkProbe.device + 1, linkProbe.attempts, linkProbe.state, (unsigned long)linkProbe.deadline,
        (unsigned long)linkProbe.requests, (unsigned long)linkProbe.confirmations,
        (unsigned long)linkProbe.failures, (unsigned long)linkProbe.receivedStatus, systemInfo.runMode,
        (unsigned long)powerSleep.stops, (unsigned long)powerSleep.rtcWakes,
        (unsigned long)powerSleep.radioWakes, (unsigned long)powerSleep.buttonWakes, powerSleep.rxPending);
      responsePrepared = 1;
    }
    if (!responsePrepared && !probePending &&
        (buttonCommandLength || memcmp(UserRxBufferFS, "BTN ", echoLength < 4 ? echoLength : 4) == 0))
    {
      uint8_t complete = 0;
      uint8_t invalid = 0;
      for (uint32_t i = 0; i < echoLength; i++)
      {
        uint8_t ch = UserRxBufferFS[i];
        if (complete || buttonCommandLength >= sizeof(buttonCommand) - 1)
        {
          invalid = 1;
          break;
        }
        buttonCommand[buttonCommandLength++] = ch;
        if (ch == '\n') complete = 1;
      }
      if (!complete && !invalid)
      {
        echoLength = 0;
        USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
        USBD_CDC_ReceivePacket(&hUsbDeviceFS);
        return;
      }
      buttonCommand[buttonCommandLength] = '\0';
      buttonCommandLength = 0;
      if (invalid)
        echoLength = sprintf((char*)UserRxBufferFS, "ERR BTN send one line, max 63 bytes\n");
      else
        CDC_ButtonCommand();
      responsePrepared = 1;
    }
    if (probePending == 2)
    {
      if (lcdProbeState != 4) { return; }
      probePending = 0;
      lcdProbeState = 0;
      echoLength = 0;
      USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
      USBD_CDC_ReceivePacket(&hUsbDeviceFS);
      return;
    }
    if (!responsePrepared && !probePending && echoLength == 7 &&
        memcmp(UserRxBufferFS, "SCREEN\n", 7) == 0)
    {
      lcdProbeState = 3;
      probePending = 2;
      return;
    }
    if (!responsePrepared && !probePending && echoLength == 9 &&
        memcmp(UserRxBufferFS, "LCDPROBE\n", 9) == 0)
    {
      lcdProbeState = 1;
      probePending = 1;
      return;
    }
    if (probePending)
    {
      if (lcdProbeState != 2) { return; }
      uint16_t offset = 0;
      for (uint8_t i = 0; i < 16; i++)
      {
        offset += sprintf((char*)UserRxBufferFS + offset, "%02X ", lcdProbeBytes[i]);
      }
      UserRxBufferFS[offset++] = '\n';
      echoLength = offset;
      probePending = 0;
      responsePrepared = 1;
      lcdProbeState = 0;
    }
    if (CDC_Transmit_FS(UserRxBufferFS, (uint16_t)echoLength) == USBD_OK)
    {
      echoTransmitting = 1;
    }
  }
}

/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @}
  */

/**
  * @}
  */
