/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "cmsis_os.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "button.h"
#include "lora.h"
#include "communication_lora.h"
#include "utility.h"
#include "config_store.h"
#include "lcd.h"
#include "screens.h"
#include "link_monitor.h"
#include "power_sleep.h"
#include "command_handlers.h"
#include "schedule.h"
#include "remote_history.h"
#include "rtc_setting.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi2_tx;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim8;

/* Definitions for systemTask */
osThreadId_t systemTaskHandle;
const osThreadAttr_t systemTask_attributes = {
  .name = "systemTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for sensorTask */
osThreadId_t sensorTaskHandle;
const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for lcdTask */
osThreadId_t lcdTaskHandle;
const osThreadAttr_t lcdTask_attributes = {
  .name = "lcdTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for buzzerTask */
osThreadId_t buzzerTaskHandle;
const osThreadAttr_t buzzerTask_attributes = {
  .name = "buzzerTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for loraSendTask */
osThreadId_t loraSendTaskHandle;
const osThreadAttr_t loraSendTask_attributes = {
  .name = "loraSendTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal1,
};
/* Definitions for loraReceiveTask */
osThreadId_t loraReceiveTaskHandle;
const osThreadAttr_t loraReceiveTask_attributes = {
  .name = "loraReceiveTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal1,
};
/* Definitions for txMsgQueue */
osMessageQueueId_t txMsgQueueHandle;
const osMessageQueueAttr_t txMsgQueue_attributes = {
  .name = "txMsgQueue"
};
/* Definitions for buzzerQueue */
osMessageQueueId_t buzzerQueueHandle;
const osMessageQueueAttr_t buzzerQueue_attributes = {
  .name = "buzzerQueue"
};
/* Definitions for GetDevStatusTimer */
osTimerId_t GetDevStatusTimerHandle;
const osTimerAttr_t GetDevStatusTimer_attributes = {
  .name = "GetDevStatusTimer"
};
/* USER CODE BEGIN PV */
static osMutexId_t uiMutex;
static osMutexId_t radioMutex;
static volatile uint32_t batterySampleSequence;
/* Buttons, polled by sensorTask and consumed by systemTask. */
Button_t leftButton = {BTN1_GPIO_Port, BTN1_Pin, eButton_State_Idle, 0, 0, 0};   ///< LEFT (PA9), wakes from STOP.
Button_t centerButton = {BTN3_GPIO_Port, BTN3_Pin, eButton_State_Idle, 0, 0, 0}; ///< CENTER (PC9).
Button_t rightButton = {BTN4_GPIO_Port, BTN4_Pin, eButton_State_Idle, 0, 0, 0};  ///< RIGHT (PC8), wakes from STOP.
Button_t onButton = {BTN2_GPIO_Port, BTN2_Pin, eButton_State_Idle, 0, 0, 0};     ///< ON (PA8).
Button_t offButton = {BTN5_GPIO_Port, BTN5_Pin, eButton_State_Idle, 0, 0, 0};    ///< OFF (PC7), wakes from STOP.
SystemInfo_t systemInfo;                 ///< Runtime state of the remote.
DeviceInfo_t deviceBuff[MAXIMUM_DEVICE]; ///< Paired devices, indexed by device id.
LoRa sLoRaCtrl;                          ///< LoRa radio driver state.
uint16_t errorCode = 0;                  ///< Result of LoRa_init() at start-up.
uint8_t rxBuffer[256];                   ///< Receive buffer of loraReceiveTask.

extern Screen_t deviceControlScreen;
Screen_t *currentScreen = &deviceControlScreen;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM3_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM8_Init(void);
void StartSystemTask(void *argument);
void StartSensorTask(void *argument);
void StartLcdTask(void *argument);
void StartBuzzerTask(void *argument);
void StartLoraSendTask(void *argument);
void StartLoraReceiveTask(void *argument);
void GetDevStatusTimerCallback(void *argument);

/* USER CODE BEGIN PFP */
static void updateBattery(void);
static uint8_t getNotUsedNodeId(void);
static void handleButton(Button_t *button, void (*clicked)(void), void (*longPressed)(void));
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  SCB->VTOR = 0x08008000u;
  __DSB();
  __ISB();

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_TIM3_Init();
  MX_RTC_Init();
  MX_TIM8_Init();
  /* USER CODE BEGIN 2 */
  LCD_Backlight_On();
  LCD_init();

  /* Start battery voltage's ADC. */
  HAL_GPIO_WritePin(BAT_CTRL_GPIO_Port, BAT_CTRL_Pin, GPIO_PIN_SET);
  HAL_Delay(5);
  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /* Sample the high-impedance divider with a long acquisition time. */
  ADC_ChannelConfTypeDef batteryChannel = {0};
  batteryChannel.Channel = ADC_CHANNEL_2;
  batteryChannel.Rank = ADC_REGULAR_RANK_1;
  batteryChannel.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &batteryChannel) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_TIM_SET_AUTORELOAD(&htim8, 999);
  __HAL_TIM_SET_COUNTER(&htim8, 0);
  HAL_ADC_Start_DMA(&hadc1, &systemInfo.batteryAdc, 1);
  HAL_TIM_Base_Start(&htim8);

  /* Load the stored configuration (newest valid slot, old format or defaults). */
  Config_Load();

  /* LoRa module settings. */
  sLoRaCtrl = newLoRa();

  sLoRaCtrl.hSPIx = &hspi1;
  sLoRaCtrl.CS_port = LORA_NSS_GPIO_Port;
  sLoRaCtrl.CS_pin = LORA_NSS_Pin;
  sLoRaCtrl.reset_port = LORA_RST_GPIO_Port;
  sLoRaCtrl.reset_pin = LORA_RST_Pin;
  sLoRaCtrl.DIO0_port = LORA_DIO_GPIO_Port;
  sLoRaCtrl.DIO0_pin = LORA_DIO_Pin;

  sLoRaCtrl.frequency = storageData.frequencyConfig;
  sLoRaCtrl.spredingFactor = storageData.sfConfig;
  sLoRaCtrl.bandWidth = BW_125KHz;          /* default = BW_125KHz */
  sLoRaCtrl.crcRate = CR_4_5;               /* default = CR_4_5 */
  sLoRaCtrl.power = POWER_17db;             /* default = 20db */
  sLoRaCtrl.overCurrentProtection = 120;    /* default = 100 mA */
  sLoRaCtrl.preamble = 10;

  LoRa_reset(&sLoRaCtrl);
  errorCode = LoRa_init(&sLoRaCtrl);
  LoRa_startReceiving(&sLoRaCtrl);

  /* System settings. */
  systemInfo.batteryVoltageMin = 3.0f;
  systemInfo.batteryVoltageMax = 4.2f;
  systemInfo.netId = storageData.netIdConfig;
  systemInfo.runMode = eSystem_RunMode_Normal;

  /* Device list. */
  for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo++)
  {
    deviceBuff[devNo].isDelete = storageData.DeviceConfig[devNo].isDelete;
    deviceBuff[devNo].nodeId = storageData.DeviceConfig[devNo].nodeIdConfig;
    deviceBuff[devNo].isDelete = storageData.DeviceConfig[devNo].isDelete;
    deviceBuff[devNo].motorData[0].alarmDelayTime = storageData.DeviceConfig[devNo].alarmDelayTime;
    deviceBuff[devNo].motorData[0].alarmThreshold = storageData.DeviceConfig[devNo].alarmThreshold;
    deviceBuff[devNo].motorData[0].isHide = storageData.DeviceConfig[devNo].isHide == TRUE;
    deviceBuff[devNo].motorData[0].isLock = storageData.DeviceConfig[devNo].isLock == TRUE;
    deviceBuff[devNo].motorData[0].moduleIndex = storageData.DeviceConfig[devNo].moduleIndex;
    deviceBuff[devNo].factoryInfo = storageData.DeviceConfig[devNo].factoryInfo;
    switch (deviceBuff[devNo].factoryInfo.deviceType)
    {
      case eDevice_Type_Motor:
        deviceBuff[devNo].factoryInfo.deviceTypeChar = 'M';
        break;
      case eDevice_Type_Pressure:
        deviceBuff[devNo].factoryInfo.deviceTypeChar = 'P';
        break;
      case eDevice_Type_Encoder:
        deviceBuff[devNo].factoryInfo.deviceTypeChar = 'E';
        break;
      default:
        deviceBuff[devNo].factoryInfo.deviceTypeChar = 'x';
        break;
    }
    memcpy(deviceBuff[devNo].motorData[0].name, storageData.DeviceConfig[devNo].name,
      sizeof(deviceBuff[devNo].motorData[0].name));
    if (deviceBuff[devNo].motorData[0].name[0] == '\0')
    {
      sprintf(deviceBuff[devNo].motorData[0].name, "Default");
    }
    deviceBuff[devNo].deviceIndex = devNo;
  }

  /* Select the first paired device. */
  HAL_RTC_GetTime(&hrtc, &systemInfo.time, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &systemInfo.date, RTC_FORMAT_BIN);
  systemInfo.currentTime = Util_GetUnixTimestamp(&systemInfo.time, &systemInfo.date);
  for (int devNo = 0; devNo < MAXIMUM_DEVICE; devNo++)
  {
    if (deviceBuff[devNo].isDelete == FALSE)
    {
      deviceBuff[devNo].lastCommTime = systemInfo.currentTime;
      systemInfo.currentDeviceId = devNo;
      systemInfo.currentModuleId = 0;
      break;
    }
  }

  RemoteHistory_Init();
  currentScreen->init();
  Link_Init(Power_ReadRtcSecond());
  Link_RequestStatusSync();
  Power_Init();
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  const osMutexAttr_t uiMutexAttributes = { .name = "UI", .attr_bits = osMutexPrioInherit };
  const osMutexAttr_t radioMutexAttributes = { .name = "LoRa", .attr_bits = osMutexPrioInherit };

  uiMutex = osMutexNew(&uiMutexAttributes);
  if (!uiMutex)
  {
    Error_Handler();
  }
  radioMutex = osMutexNew(&radioMutexAttributes);
  if (!radioMutex)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* Create the timer(s) */
  /* creation of GetDevStatusTimer */
  GetDevStatusTimerHandle = osTimerNew(GetDevStatusTimerCallback, osTimerPeriodic, NULL, &GetDevStatusTimer_attributes);

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of txMsgQueue */
  txMsgQueueHandle = osMessageQueueNew (16, sizeof(LoraPackage_t), &txMsgQueue_attributes);

  /* creation of buzzerQueue */
  buzzerQueueHandle = osMessageQueueNew (16, sizeof(Buzzer_Mode_e), &buzzerQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of systemTask */
  systemTaskHandle = osThreadNew(StartSystemTask, NULL, &systemTask_attributes);

  /* creation of sensorTask */
  sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);

  /* creation of lcdTask */
  lcdTaskHandle = osThreadNew(StartLcdTask, NULL, &lcdTask_attributes);

  /* creation of buzzerTask */
  buzzerTaskHandle = osThreadNew(StartBuzzerTask, NULL, &buzzerTask_attributes);

  /* creation of loraSendTask */
  loraSendTaskHandle = osThreadNew(StartLoraSendTask, NULL, &loraSendTask_attributes);

  /* creation of loraReceiveTask */
  loraReceiveTaskHandle = osThreadNew(StartLoraReceiveTask, NULL, &loraReceiveTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_ADC
                              |RCC_PERIPHCLK_USB;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_AnalogWDGConfTypeDef AnalogWDGConfig = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T8_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable or disable the remapping of ADC1_ETRGREG:
  * ADC1 External Event regular conversion is connected to TIM8 TRG0
  */
  __HAL_AFIO_REMAP_ADC1_ETRGREG_ENABLE();

  /** Configure Analog WatchDog 1
  */
  AnalogWDGConfig.WatchdogMode = ADC_ANALOGWATCHDOG_SINGLE_REG;
  AnalogWDGConfig.HighThreshold = 0;
  AnalogWDGConfig.LowThreshold = 0;
  AnalogWDGConfig.Channel = ADC_CHANNEL_2;
  AnalogWDGConfig.ITMode = DISABLE;
  if (HAL_ADC_AnalogWDGConfig(&hadc1, &AnalogWDGConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_7CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef DateToUpdate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_ALARM;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
  /* Only set the default time/date when the RTC was not configured before. */
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != 0x3210)
  {
  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0;
  sTime.Minutes = 0;
  sTime.Seconds = 0;

  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  DateToUpdate.WeekDay = RTC_WEEKDAY_SATURDAY;
  DateToUpdate.Month = RTC_MONTH_JANUARY;
  DateToUpdate.Date = 1;
  DateToUpdate.Year = 0;

  if (HAL_RTC_SetDate(&hrtc, &DateToUpdate, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0x3210);
  }
  Rtc_RestoreDate();
  /* USER CODE END RTC_Init 2 */
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_1LINE;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 365;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 183;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM8 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM8_Init(void)
{

  /* USER CODE BEGIN TIM8_Init 0 */

  /* USER CODE END TIM8_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM8_Init 1 */

  /* USER CODE END TIM8_Init 1 */
  htim8.Instance = TIM8;
  htim8.Init.Prescaler = 7199;
  htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim8.Init.Period = 10000;
  htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim8.Init.RepetitionCounter = 0;
  htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim8) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim8, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM8_Init 2 */

  /* USER CODE END TIM8_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, BAT_CTRL_Pin|LORA_NSS_Pin|USB_CTRL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LED1_Pin|LED2_Pin|LCD_BLK_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_RST_Pin|LCD_CFG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LORA_RST_Pin LED1_Pin LED2_Pin LCD_BLK_Pin */
  GPIO_InitStruct.Pin = LORA_RST_Pin|LED1_Pin|LED2_Pin|LCD_BLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LORA_DIO_Pin BTN4_Pin */
  GPIO_InitStruct.Pin = LORA_DIO_Pin|BTN4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : BAT_CTRL_Pin LORA_NSS_Pin USB_CTRL_Pin */
  GPIO_InitStruct.Pin = BAT_CTRL_Pin|LORA_NSS_Pin|USB_CTRL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI2_CS_Pin LCD_RST_Pin LCD_CFG_Pin */
  GPIO_InitStruct.Pin = SPI2_CS_Pin|LCD_RST_Pin|LCD_CFG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN5_Pin BTN3_Pin */
  GPIO_InitStruct.Pin = BTN5_Pin|BTN3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN2_Pin */
  GPIO_InitStruct.Pin = BTN2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BTN2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN1_Pin */
  GPIO_InitStruct.Pin = BTN1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BTN1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BAT_STDBY_Pin BAT_CHARGING_Pin */
  GPIO_InitStruct.Pin = BAT_STDBY_Pin|BAT_CHARGING_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
 * @brief  Count a finished battery sample (ADC1 by DMA, triggered by TIM8).
 * @param  adc  ADC that finished.
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc)
{
  if (adc->Instance == ADC1)
  {
    batterySampleSequence++;
  }
}

/**
 * @brief  EXTI wake sources: LEFT/RIGHT/OFF buttons and LoRa DIO0.
 *
 * DIO0 also notifies loraReceiveTask, except while Power_TryStop() is
 * entering STOP; it notifies the task itself after wake-up.
 *
 * @param  GPIO_Pin  Pin that triggered.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  BaseType_t higherPriorityTaskWoken = pdFALSE;

  if (GPIO_Pin == BTN1_Pin || GPIO_Pin == BTN4_Pin || GPIO_Pin == BTN5_Pin)
  {
    Power_ButtonWakeISR();
  }
  else if (GPIO_Pin == sLoRaCtrl.DIO0_pin)
  {
    Power_RadioWakeISR();
    if (loraReceiveTaskHandle && !powerSleep.stopping)
    {
      vTaskNotifyGiveFromISR(loraReceiveTaskHandle, &higherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
  }
}

/**
 * @brief  RTC Alarm A: wake-up for the periodic sleep scan.
 * @param  hrtc  RTC handle.
 */
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
  Power_RtcWakeISR();
}

/**
 * @brief  Ask buzzerTask to play a pattern.
 * @param  mode  Pattern to play.
 * @note   Does not block; the request is dropped if the queue is full.
 */
void Main_SetBuzzerMode(Buzzer_Mode_e mode)
{
  osMessageQueuePut(buzzerQueueHandle, &mode, 0, 0);
}

/**
 * @brief  Send CMD_CONTROL_POWER to the selected device and module.
 *
 * A locked motor is refused with a long beep and the lock notice; an
 * invalid, hidden or deleted device is ignored. Cancels the tracked schedule
 * of that node, since the command replaces it.
 *
 * @param  motorPower  A Motor_PowerState_e value.
 */
void Main_SetMotorPower(uint8_t motorPower)
{
  /* Guard the common send path for both physical and USB-injected buttons. */
  if (systemInfo.currentDeviceId >= MAXIMUM_DEVICE ||
    systemInfo.currentModuleId >= sizeof(deviceBuff[0].motorData) / sizeof(deviceBuff[0].motorData[0]))
  {
    return;
  }
  if (!ScrnCommon_DeviceVisible(systemInfo.currentDeviceId))
  {
    return;
  }
  if (deviceBuff[systemInfo.currentDeviceId].motorData[systemInfo.currentModuleId].isLock)
  {
    ScrnDcs_ShowMotorLocked(motorPower);
    Main_SetBuzzerMode(eBuzzer_Mode_LongBeep);
    return;
  }

  if (deviceBuff[systemInfo.currentDeviceId].isDelete == FALSE)
  {
    Schedule_CancelForNode(deviceBuff[systemInfo.currentDeviceId].nodeId);
    LoraPackage_t packageOut = Comm_CreateReqControlPower(systemInfo.netId, deviceBuff[systemInfo.currentDeviceId].nodeId,
      systemInfo.currentModuleId, motorPower);
    osMessageQueuePut(txMsgQueueHandle, &packageOut, 0, pdMS_TO_TICKS(100));
  }
}

/** @brief Apply a new status period: stop the legacy poll timer and request a link scan. */
void Main_UpdateStatusPeriod(void)
{
  /* Compatibility entry point: scanning is local, never unconditional RF polling. */
  if (GetDevStatusTimerHandle)
  {
    osTimerStop(GetDevStatusTimerHandle);
  }
  Link_RequestScan();
}

/**
 * @brief  Send CMD_SET_SCHEDULE for the selected device and track it.
 * @param  time  Seconds from now until switch-off; negative cancels the schedule.
 */
void Main_SetScheduleTimeToCurrentModule(int32_t time)
{
  PayloadRequestSetSchedule_t payload;

  if (!ScrnCommon_DeviceVisible(systemInfo.currentDeviceId))
  {
    return;
  }

  payload.moduleIndex = systemInfo.currentModuleId;
  payload.syncTime = systemInfo.currentTime;
  if (time < 0)
  {
    payload.scheduleTime = 0;
  }
  else
  {
    payload.scheduleTime = systemInfo.currentTime + time;
  }

  LoraPackage_t packageOut = Comm_CreateReqSetSchedule(systemInfo.netId, deviceBuff[systemInfo.currentDeviceId].nodeId, payload);
  Schedule_Track(&packageOut, &payload);
  if (osMessageQueuePut(txMsgQueueHandle, &packageOut, 0, pdMS_TO_TICKS(100)) != osOK)
  {
    Schedule_QueueFailed();
  }
}

/**
 * @brief  Start pairing: switch the radio to the default settings and ask for factory info.
 *
 * Sets 433 MHz, SF7 and the default network id, then queues
 * CMD_GET_FACTORY_INFO. systemTask reports the result on the pairing screen,
 * or "not found" after 5 s.
 */
void Main_ScanNewDevice(void)
{
  FactoryInfo_t factoryInfo;

  osMutexAcquire(radioMutex, osWaitForever);
  systemInfo.findDeviceTick = xTaskGetTickCount();
  systemInfo.findDeviceStep = eProcess_Step_Processing;
  systemInfo.findDeviceResult = eProcess_Result_Failed;
  systemInfo.findDevicefactoryInfo = factoryInfo;
  systemInfo.netId = LORA_NET_ID_DEFAULT;
  sLoRaCtrl.frequency = 433;
  sLoRaCtrl.spredingFactor = SF_7;
  LoRa_init(&sLoRaCtrl);
  LoRa_startReceiving(&sLoRaCtrl);
  osMutexRelease(radioMutex);

  LoraPackage_t packageOut = Comm_CreateReqGetFactoryInfo(LORA_NET_ID_DEFAULT, LORA_NODE_ID_DEFAULT);
  osMessageQueuePut(txMsgQueueHandle, &packageOut, 0, pdMS_TO_TICKS(100));
}

/**
 * @brief  Send CMD_CONFIG_CONNECTION to the device found by Main_ScanNewDevice().
 *
 * Gives it the configured network id, radio settings and the next unused
 * node id. systemTask stores the device when it answers, or fails after 5 s.
 */
void Main_ConnectToDevice(void)
{
  PayloadRequestConfigConnection_t payload;

  systemInfo.configDeviceTick = xTaskGetTickCount();
  systemInfo.configDeviceStep = eProcess_Step_Processing;
  systemInfo.configDeviceResult = eProcess_Result_Failed;
  systemInfo.nextNodeId = getNotUsedNodeId();

  payload.serial = systemInfo.findDevicefactoryInfo.serial;
  payload.netId = storageData.netIdConfig;
  payload.nodeId = systemInfo.nextNodeId;
  payload.frequency = storageData.frequencyConfig;
  payload.sf = storageData.sfConfig;

  LoraPackage_t packageOut = Comm_CreateReqConfigConnection(LORA_NET_ID_DEFAULT, LORA_NODE_ID_DEFAULT, payload);
  osMessageQueuePut(txMsgQueueHandle, &packageOut, 0, pdMS_TO_TICKS(100));
}

/**
 * @brief  Arm RTC Alarm A some seconds from now.
 * @param  seconds  Delay [s]; the alarm time wraps at midnight.
 */
void Main_SetRtcAlarmRelative(uint32_t seconds)
{
  RTC_AlarmTypeDef sAlarm = {0};
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};
  uint32_t alarmSeconds;
  uint32_t alarmMinutes;
  uint32_t alarmHours;

  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  alarmSeconds = sTime.Seconds + seconds;
  alarmMinutes = sTime.Minutes + (alarmSeconds / 60);
  alarmHours = sTime.Hours + (alarmMinutes / 60);

  sAlarm.AlarmTime.Seconds = alarmSeconds % 60;
  sAlarm.AlarmTime.Minutes = alarmMinutes % 60;
  sAlarm.AlarmTime.Hours = alarmHours % 24;
  sAlarm.Alarm = RTC_ALARM_A;
  HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN);
}

static void updateBattery(void)
{
  static uint32_t lastSequence;
  static float filtered;
  static const float volts[] = {3.0f, 3.3f, 3.5f, 3.6f, 3.7f, 3.8f, 3.9f, 4.0f, 4.1f, 4.2f};
  static const uint8_t levels[] = {0, 5, 10, 20, 40, 60, 75, 85, 95, 100};
  uint32_t sequence;
  float measured;
  float percent;

  sequence = batterySampleSequence;
  if (sequence == lastSequence)
  {
    return;
  }
  lastSequence = sequence;

  /* R15 = R18 = 10k, nominal VDDA from the regulated 3.3V rail. */
  measured = (float)(systemInfo.batteryAdc & 0xFFFFu) * 3.3f * 2.0f / 4095.0f;
  if (measured < 2.5f || measured > 4.5f)
  {
    filtered = 0;
    systemInfo.batteryVoltage = 0;
    systemInfo.batteryUsage = 0;
    return;
  }
  filtered = filtered == 0 ? measured : filtered + 0.1f * (measured - filtered);
  systemInfo.batteryVoltage = filtered;

  percent = 0;
  if (filtered >= volts[9])
  {
    percent = 100;
  }
  else
  {
    for (uint8_t i = 1; i < 10; i++)
    {
      if (filtered <= volts[i])
      {
        percent = levels[i - 1] + (filtered - volts[i - 1]) *
          (levels[i] - levels[i - 1]) / (volts[i] - volts[i - 1]);
        break;
      }
    }
  }
  if (percent < 0)
  {
    percent = 0;
  }
  if (percent > 100)
  {
    percent = 100;
  }
  if (systemInfo.isFullBattery && !systemInfo.isChanging && filtered >= 4.05f)
  {
    percent = 100;
  }
  systemInfo.batteryUsage = (uint8_t)(percent + 0.5f);
}

static uint8_t getNotUsedNodeId(void)
{
  uint8_t nodeIdValid = 0x00;
  uint16_t wasNodeIdUsed;

  for (uint8_t nodeId = 0x01; nodeId <= 255; nodeId++)
  {
    wasNodeIdUsed = FALSE;
    if (nodeId == LORA_NODE_ID_BROADCAST || nodeId == LORA_NET_ID_DEFAULT)
    {
      continue;
    }
    for (uint8_t devNo = 0; devNo < MAXIMUM_DEVICE; devNo++)
    {
      if (deviceBuff[devNo].isDelete == 0x00 && deviceBuff[devNo].nodeId == nodeId)
      {
        wasNodeIdUsed = TRUE;
        break;
      }
    }
    if (wasNodeIdUsed == FALSE)
    {
      nodeIdValid = nodeId;
      break;
    }
  }
  return nodeIdValid;
}

/* Run the screen handler for a click or long press and clear its flag. */
static void handleButton(Button_t *button, void (*clicked)(void), void (*longPressed)(void))
{
  if (button->isPressed)
  {
    button->isPressed = 0;
    clicked();
  }
  else if (button->isLongPressed)
  {
    button->isLongPressed = 0;
    longPressed();
  }
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartSystemTask */
/**
  * @brief  Function implementing the systemTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartSystemTask */
void StartSystemTask(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 5 */
  uint8_t isFullDev;
  uint8_t devIndexValid;

  /* Infinite loop */
  for (;;)
  {
    Link_ClockStep(Power_ReadRtcSecond());
    /* Screen changes, redraws and LCD readback share the same widgets/SPI.
       Never delete a window while lcdTask is inside its render callback. */
    osMutexAcquire(uiMutex, osWaitForever);
    Power_Tick();
    CDC_ProcessEcho_FS();

    /* Read the date time value. */
    HAL_RTC_GetTime(&hrtc, &systemInfo.time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &systemInfo.date, RTC_FORMAT_BIN);
    systemInfo.currentTime = Util_GetUnixTimestamp(&systemInfo.time, &systemInfo.date);
    Rtc_SaveDate();
    updateBattery();

    /* Buttons. */
    if (leftButton.isPressed || leftButton.isLongPressed || rightButton.isPressed ||
      rightButton.isLongPressed || centerButton.isPressed || centerButton.isLongPressed ||
      onButton.isPressed || onButton.isLongPressed || offButton.isPressed || offButton.isLongPressed)
    {
      Power_UserActivity();
    }
    handleButton(&leftButton, currentScreen->leftButtonClickedCallback, currentScreen->leftButtonPressedCallback);
    handleButton(&rightButton, currentScreen->rightButtonClickedCallback, currentScreen->rightButtonPressedCallback);
    handleButton(&centerButton, currentScreen->centerButtonClickedCallback, currentScreen->centerButtonPressedCallback);
    handleButton(&onButton, currentScreen->bleftButtonClickedCallback, currentScreen->bleftButtonPressedCallback);
    handleButton(&offButton, currentScreen->brightButtonClickedCallback, currentScreen->brightButtonPressedCallback);

    /* Device search. */
    if (systemInfo.findDeviceStep == eProcess_Step_Done)
    {
      if (currentScreen == &devicePairingScreen && pairingUI_Step == PAIRING_STATE_SEARCHING &&
        systemInfo.findDeviceResult == eProcess_Result_Passed)
      {
        pairingUI_Step = PAIRING_STATE_FOUND;
        currentScreen = &devicePairingScreen;
        currentScreen->init();
      }
      systemInfo.findDeviceStep = eProcess_Step_None;
    }
    else if (systemInfo.findDeviceStep == eProcess_Step_Processing)
    {
      if (xTaskGetTickCount() - systemInfo.findDeviceTick > pdMS_TO_TICKS(5000))
      {
        if (currentScreen == &devicePairingScreen && pairingUI_Step == PAIRING_STATE_SEARCHING)
        {
          pairingUI_Step = PAIRING_STATE_NOT_FOUND;
          currentScreen = &devicePairingScreen;
          currentScreen->init();
        }
        systemInfo.findDeviceStep = eProcess_Step_None;
      }
    }

    /* Device connection. */
    if (systemInfo.configDeviceStep == eProcess_Step_Done)
    {
      if (currentScreen == &devicePairingScreen && pairingUI_Step == PAIRING_STATE_CONNECTING &&
        systemInfo.configDeviceResult == eProcess_Result_Passed)
      {
        isFullDev = TRUE;
        for (uint8_t devNo = 0; devNo < MAXIMUM_DEVICE; devNo++)
        {
          if (deviceBuff[devNo].isDelete == TRUE)
          {
            devIndexValid = devNo;
            isFullDev = FALSE;
            break;
          }
        }
        if (isFullDev == FALSE)
        {
          deviceBuff[devIndexValid].factoryInfo = systemInfo.findDevicefactoryInfo;
          deviceBuff[devIndexValid].nodeId = systemInfo.nextNodeId;
          deviceBuff[devIndexValid].motorData[0].isHide = FALSE;
          deviceBuff[devIndexValid].motorData[0].isLock = FALSE;
          deviceBuff[devIndexValid].isDelete = FALSE;
          deviceBuff[devIndexValid].lastCommTime = systemInfo.currentTime;

          storageData.DeviceConfig[devIndexValid].factoryInfo = deviceBuff[devIndexValid].factoryInfo;
          storageData.DeviceConfig[devIndexValid].nodeIdConfig = deviceBuff[devIndexValid].nodeId;
          storageData.DeviceConfig[devIndexValid].isHide = deviceBuff[devIndexValid].motorData[0].isHide;
          storageData.DeviceConfig[devIndexValid].isLock = deviceBuff[devIndexValid].motorData[0].isLock;
          storageData.DeviceConfig[devIndexValid].isDelete = deviceBuff[devIndexValid].isDelete;

          systemInfo.isNeedSaveConfig = TRUE;

          pairingUI_Step = PAIRING_STATE_SUCCESS;
          currentScreen = &devicePairingScreen;
          currentScreen->init();
        }
        else
        {
          pairingUI_Step = PAIRING_STATE_FAILED;
          currentScreen = &devicePairingScreen;
          currentScreen->init();
        }
      }
      systemInfo.configDeviceStep = eProcess_Step_None;
    }
    else if (systemInfo.configDeviceStep == eProcess_Step_Processing)
    {
      if (xTaskGetTickCount() - systemInfo.configDeviceTick > pdMS_TO_TICKS(5000))
      {
        if (currentScreen == &devicePairingScreen && pairingUI_Step == PAIRING_STATE_CONNECTING)
        {
          pairingUI_Step = PAIRING_STATE_FAILED;
          currentScreen = &devicePairingScreen;
          currentScreen->init();
        }
        systemInfo.configDeviceStep = eProcess_Step_None;
      }
    }

    /* Connection state is owned by Link_Tick(), including sleep-aware retries. */

    /* Persist configuration. */
    if (systemInfo.isNeedSaveConfig == TRUE)
    {
      systemInfo.isNeedSaveConfig = FALSE;
      if (Config_Save())
      {
        Main_SetBuzzerMode(eBuzzer_Mode_LongBeep);
      }
    }

    RemoteHistory_Tick();
    Schedule_Tick();
    if (!powerSleep.rxPending)
    {
      Link_Tick();
    }
    Power_TryStop();
    osMutexRelease(uiMutex);
    osDelay(pdMS_TO_TICKS(100));
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartSensorTask */
/**
* @brief Function implementing the sensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSensorTask */
void StartSensorTask(void *argument)
{
  /* USER CODE BEGIN StartSensorTask */
  /* Infinite loop */
  for (;;)
  {
    Button_UpdateState(&rightButton);
    Button_UpdateState(&centerButton);
    Button_UpdateState(&leftButton);
    Button_UpdateState(&onButton);
    Button_UpdateState(&offButton);

    /* Read battery charger status. */
    systemInfo.isChanging = HAL_GPIO_ReadPin(BAT_CHARGING_GPIO_Port, BAT_CHARGING_Pin) == GPIO_PIN_RESET ? 1 : 0;
    systemInfo.isFullBattery = HAL_GPIO_ReadPin(BAT_STDBY_GPIO_Port, BAT_STDBY_Pin) == GPIO_PIN_RESET ? 1 : 0;
    osDelay(1);
  }
  /* USER CODE END StartSensorTask */
}

/* USER CODE BEGIN Header_StartLcdTask */
/**
* @brief Function implementing the lcdTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartLcdTask */
void StartLcdTask(void *argument)
{
  /* USER CODE BEGIN StartLcdTask */
  /* Infinite loop */
  for (;;)
  {
    osMutexAcquire(uiMutex, osWaitForever);
    if (systemInfo.runMode != eSystem_RunMode_Sleep)
    {
      currentScreen->render();
    }
    if (LCD_Recover())
    {
      /* Redraw existing widgets on next tick. */
      UG_WindowShow(&window_1);
    }
    LCD_ProcessProbe();
    osMutexRelease(uiMutex);
    osDelay(200);
  }
  /* USER CODE END StartLcdTask */
}

/* USER CODE BEGIN Header_StartBuzzerTask */
/**
* @brief Function implementing the buzzerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartBuzzerTask */
void StartBuzzerTask(void *argument)
{
  /* USER CODE BEGIN StartBuzzerTask */
  TIM_HandleTypeDef *buzzerTimer = &htim3;
  const uint32_t BUZZER_TIMER_CHANNEL = TIM_CHANNEL_3;
  const float MAX_CC_VALUE = 183.0f;
  Buzzer_Mode_e mode;

  /* Infinite loop */
  for (;;)
  {
    if (osMessageQueueGet(buzzerQueueHandle, &mode, NULL, osWaitForever) == osOK)
    {
      switch (mode)
      {
        case eBuzzer_Mode_Off:
          HAL_TIM_PWM_Stop(buzzerTimer, BUZZER_TIMER_CHANNEL);
          break;
        case eBuzzer_Mode_ShortBeep:
          __HAL_TIM_SET_COMPARE(buzzerTimer, BUZZER_TIMER_CHANNEL, MAX_CC_VALUE * 0.3f);
          HAL_TIM_PWM_Start(buzzerTimer, BUZZER_TIMER_CHANNEL);
          osDelay(100);
          HAL_TIM_PWM_Stop(buzzerTimer, BUZZER_TIMER_CHANNEL);
          break;
        case eBuzzer_Mode_2ShortBeep:
          __HAL_TIM_SET_COMPARE(buzzerTimer, BUZZER_TIMER_CHANNEL, MAX_CC_VALUE * 0.3f);
          for (int i = 0; i < 2; i++)
          {
            HAL_TIM_PWM_Start(buzzerTimer, BUZZER_TIMER_CHANNEL);
            osDelay(100);
            HAL_TIM_PWM_Stop(buzzerTimer, BUZZER_TIMER_CHANNEL);
            osDelay(100);
          }
          break;
        case eBuzzer_Mode_3ShortBeep:
          __HAL_TIM_SET_COMPARE(buzzerTimer, BUZZER_TIMER_CHANNEL, MAX_CC_VALUE * 0.3f);
          for (int i = 0; i < 3; i++)
          {
            HAL_TIM_PWM_Start(buzzerTimer, BUZZER_TIMER_CHANNEL);
            osDelay(100);
            HAL_TIM_PWM_Stop(buzzerTimer, BUZZER_TIMER_CHANNEL);
            osDelay(100);
          }
          break;
        case eBuzzer_Mode_LongBeep:
          __HAL_TIM_SET_COMPARE(buzzerTimer, BUZZER_TIMER_CHANNEL, MAX_CC_VALUE * 0.8f);
          HAL_TIM_PWM_Start(buzzerTimer, BUZZER_TIMER_CHANNEL);
          osDelay(300);
          HAL_TIM_PWM_Stop(buzzerTimer, BUZZER_TIMER_CHANNEL);
          break;
        case eBuzzer_Mode_AlarmLong:
          __HAL_TIM_SET_COMPARE(buzzerTimer, BUZZER_TIMER_CHANNEL, MAX_CC_VALUE * 1.0f);
          HAL_TIM_PWM_Start(buzzerTimer, BUZZER_TIMER_CHANNEL);
          break;
        default:
          break;
      }
    }
    osDelay(100);
  }
  /* USER CODE END StartBuzzerTask */
}

/* USER CODE BEGIN Header_StartLoraSendTask */
/**
* @brief Function implementing the loraSendTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartLoraSendTask */
void StartLoraSendTask(void *argument)
{
  /* USER CODE BEGIN StartLoraSendTask */
  osStatus_t queueStatus;
  LoraPackage_t txPackage;
  uint8_t sent;

  /* Link supervisor is the sole source of automatic GET_STATUS requests. */
  /* Infinite loop */
  for (;;)
  {
    queueStatus = osMessageQueueGet(txMsgQueueHandle, &txPackage, NULL, osWaitForever);
    if (queueStatus == osOK)
    {
      if (!Link_ShouldTransmit(&txPackage) || !Schedule_ShouldTransmit(&txPackage))
      {
        continue;
      }
      osMutexAcquire(radioMutex, osWaitForever);
      powerSleep.txBusy = 1;
      sent = LoRa_transmit(&sLoRaCtrl, txPackage.rawData, txPackage.byteCount, 5000);
      LoRa_startReceiving(&sLoRaCtrl);
      powerSleep.txBusy = 0;
      osMutexRelease(radioMutex);

      Link_TxFinished(&txPackage, sent);
      Schedule_TxFinished(&txPackage, sent);
      osDelay(pdMS_TO_TICKS(200));
    }
  }
  /* USER CODE END StartLoraSendTask */
}

/* USER CODE BEGIN Header_StartLoraReceiveTask */
/**
* @brief Function implementing the loraReceiveTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartLoraReceiveTask */
void StartLoraReceiveTask(void *argument)
{
  /* USER CODE BEGIN StartLoraReceiveTask */
  uint32_t notificationValue;
  uint8_t rxLength;

  /* Infinite loop */
  for (;;)
  {
    notificationValue = powerSleep.rxPending ? 1 : ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    if (notificationValue > 0)
    {
      osMutexAcquire(radioMutex, osWaitForever);
      rxLength = LoRa_receive(&sLoRaCtrl, rxBuffer, 128);
      Comm_ProcessRxData(rxBuffer, rxLength);
      Power_RxProcessed();
      osMutexRelease(radioMutex);
    }
  }
  /* USER CODE END StartLoraReceiveTask */
}

/* GetDevStatusTimerCallback function */
void GetDevStatusTimerCallback(void *argument)
{
  /* USER CODE BEGIN GetDevStatusTimerCallback */
  Link_RequestScan();
  /* USER CODE END GetDevStatusTimerCallback */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
