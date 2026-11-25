#include "power_sleep.h"
#include "remote_history.h"
#include "schedule.h"
#include "link_monitor.h"
#include "screens.h"
#include "main.h"
#include "cmsis_os.h"
#include "usbd_def.h"
#include <string.h>

extern SystemInfo_t systemInfo;
extern osMessageQueueId_t txMsgQueueHandle;
extern osMessageQueueId_t buzzerQueueHandle;
extern osThreadId_t loraReceiveTaskHandle;
extern USBD_HandleTypeDef hUsbDeviceFS;
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim8;
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;

Power_SleepState_t powerSleep;

static uint8_t usbActive(void);
static uint8_t buttonsDown(void);
static void restoreClock(uint32_t cfgr);

/* RTC clock ================================================================ */

uint32_t Power_ReadRtcSecond(void)
{
    uint32_t first;
    uint32_t second;
    uint32_t low;
    do
    {
        first = RTC->CNTH;
        low = RTC->CNTL;
        second = RTC->CNTH;
    } while (first != second);
    return ((second << 16) | low) % 86400u;
}

/* Initialisation and activity ============================================== */

void Power_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    memset(&powerSleep, 0, sizeof(powerSleep));
    powerSleep.lastUser = Link_Now();
    /* Three non-conflicting EXTI lines: LEFT PA9, RIGHT PC8, OFF PC7.
       CENTER PC9 / ON PA8 share lines 9/8 and cannot also wake from STOP. */
    gpio.Pin = BTN5_Pin;
    gpio.Mode = GPIO_MODE_IT_RISING;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BTN5_GPIO_Port, &gpio);
    if (HAL_GPIO_ReadPin(LORA_DIO_GPIO_Port, LORA_DIO_Pin))
    {
        powerSleep.rxPending = 1;
    }
}

void Power_UserActivity(void)
{
    powerSleep.lastUser = Link_Now();
    systemInfo.lastActionTick = xTaskGetTickCount();
    systemInfo.runMode = SYS_MODE_NORMAL;
    LCD_Backlight_On();
}

void Power_WakeForError(void)
{
    systemInfo.runMode = SYS_MODE_ALARM;
    LCD_Backlight_On();
}

/* Wake sources (ISR context) =============================================== */

void Power_RtcWakeISR(void)
{
    powerSleep.rtcPending = 1;
}

void Power_RadioWakeISR(void)
{
    powerSleep.rxPending = 1;
}

void Power_ButtonWakeISR(void)
{
    powerSleep.buttonPending = 1;
}

void Power_UsbSofISR(void)
{
    powerSleep.lastUsbSof = HAL_GetTick();
}

void Power_RxProcessed(void)
{
    powerSleep.rxPending = 0;
    Link_RequestScan();
}

/* Sleep policy ============================================================= */

void Power_Tick(void)
{
    if (powerSleep.buttonPending)
    {
        powerSleep.buttonPending = 0;
        Power_UserActivity();
    }
    if (powerSleep.rtcPending)
    {
        powerSleep.rtcPending = 0;
        powerSleep.nextRtc = Link_Now() + systemInfo.tSleepUpdate;
        Link_RequestScan();
    }
    if (systemInfo.runMode == SYS_MODE_SLEEP && (!systemInfo.sleepMode || usbActive()))
    {
        Power_UserActivity();
    }
    if (!systemInfo.sleepMode || usbActive() || systemInfo.runMode == SYS_MODE_ALARM ||
        currentScreen == &editDeviceSettingScreen || currentScreen == &editGeneralSettingScreen ||
        currentScreen == &dateTimeScreen ||
        ScrnDcs_IsScheduleEditing() || Schedule_IsWaiting() || RemoteHistory_Busy() ||
        currentScreen == &devicePairingScreen || systemInfo.isNeedSaveConfig || buttonsDown())
    {
        return;
    }
    if (systemInfo.runMode != SYS_MODE_SLEEP &&
        Link_Now() - powerSleep.lastUser >= Link_GetIdleSeconds())
    {
        systemInfo.runMode = SYS_MODE_SLEEP;
        powerSleep.nextRtc = Link_Now() + systemInfo.tSleepUpdate;
        LCD_Backlight_Off();
    }
}

void Power_TryStop(void)
{
    uint32_t now;
    uint32_t remaining;
    uint32_t systick;
    uint32_t cfgr;
    uint32_t cause;

    if (systemInfo.runMode != SYS_MODE_SLEEP || Link_IsBusy() || powerSleep.rxPending ||
        powerSleep.txBusy || usbActive() || systemInfo.isNeedSaveConfig ||
        osMessageQueueGetCount(txMsgQueueHandle) || osMessageQueueGetCount(buzzerQueueHandle) ||
        (TIM3->CCER & TIM_CCER_CC3E) || hspi1.State != HAL_SPI_STATE_READY ||
        hspi2.State != HAL_SPI_STATE_READY || buttonsDown())
    {
        return;
    }

    /* Arm the RTC wake-up and stop peripherals that must not run in STOP. */
    now = Link_Now();
    remaining = (int32_t)(powerSleep.nextRtc - now) > 0 ? powerSleep.nextRtc - now : 1;
    Main_SetRtcAlarmRelative(remaining);
    HAL_TIM_Base_Stop(&htim8);
    HAL_ADC_Stop_DMA(&hadc1);

    /* Freeze the scheduler and SysTick. */
    vTaskSuspendAll();
    powerSleep.stopping = 1;
    __disable_irq();
    systick = SysTick->CTRL;
    cfgr = RCC->CFGR;
    SysTick->CTRL = 0;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    HAL_SuspendTick();

    /* Enter STOP unless a wake-up source is already pending. */
    if (!powerSleep.rxPending && !powerSleep.buttonPending &&
        !(EXTI->PR & (LORA_DIO_Pin | BTN1_Pin | BTN4_Pin | BTN5_Pin | (1u << 17))))
    {
        __HAL_RCC_PWR_CLK_ENABLE();
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
        powerSleep.stops++;
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

        cause = EXTI->PR;
        if (cause & LORA_DIO_Pin)
        {
            powerSleep.radioWakes++;
        }
        if (cause & (1u << 17))
        {
            powerSleep.rtcWakes++;
        }
        if (cause & (BTN1_Pin | BTN4_Pin | BTN5_Pin))
        {
            powerSleep.buttonWakes++;
        }
        restoreClock(cfgr);
    }

    /* Resume SysTick and the scheduler. */
    Link_ClockStep(Power_ReadRtcSecond());
    HAL_ResumeTick();
    SysTick->VAL = 0;
    SysTick->CTRL = systick;
    __enable_irq();
    powerSleep.stopping = 0;
    xTaskResumeAll();

    /* RTOS time intentionally pauses in STOP; health/deadlines use RTC uptime. */
    if (powerSleep.rxPending)
    {
        xTaskNotifyGive(loraReceiveTaskHandle);
    }

    /* Restart battery sampling. */
    __HAL_TIM_SET_COUNTER(&htim8, 0);
    HAL_ADC_Start_DMA(&hadc1, &systemInfo.batteryAdc, 1);
    HAL_TIM_Base_Start(&htim8);
}

/* Private helpers ========================================================== */

static uint8_t usbActive(void)
{
    return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED &&
        (uint32_t)(HAL_GetTick() - powerSleep.lastUsbSof) < 2000;
}

static uint8_t buttonsDown(void)
{
    return HAL_GPIO_ReadPin(BTN1_GPIO_Port, BTN1_Pin) || HAL_GPIO_ReadPin(BTN2_GPIO_Port, BTN2_Pin) ||
        HAL_GPIO_ReadPin(BTN3_GPIO_Port, BTN3_Pin) || HAL_GPIO_ReadPin(BTN4_GPIO_Port, BTN4_Pin) ||
        HAL_GPIO_ReadPin(BTN5_GPIO_Port, BTN5_Pin);
}

/* Bounded clock recovery with interrupts masked: no RTOS/HAL timeout runs
   on the temporary HSI clock. Peripheral/USB IRQs execute only after recovery. */
static void restoreClock(uint32_t cfgr)
{
    uint32_t budget = 4000000;
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY) && --budget)
    {
        /* Wait for HSE. */
    }
    if (!budget)
    {
        NVIC_SystemReset();
    }
    RCC->CFGR = cfgr & ~RCC_CFGR_SW;
    RCC->CR |= RCC_CR_PLLON;
    budget = 4000000;
    while (!(RCC->CR & RCC_CR_PLLRDY) && --budget)
    {
        /* Wait for PLL lock. */
    }
    if (!budget)
    {
        NVIC_SystemReset();
    }
    RCC->CFGR = cfgr;
    budget = 4000000;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL && --budget)
    {
        /* Wait for the PLL to become the system clock. */
    }
    if (!budget)
    {
        NVIC_SystemReset();
    }
}
