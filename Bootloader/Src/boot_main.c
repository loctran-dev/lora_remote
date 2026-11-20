#include "main.h"
#include "boot_layout.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
extern PCD_HandleTypeDef hpcd_USB_FS;

uint32_t Boot_CRC32(const void *buffer, uint32_t size)
{
  const uint8_t *data = buffer;
  uint32_t crc = 0xFFFFFFFFu;
  while (size--)
  {
    crc ^= *data++;
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0);
  }
  return ~crc;
}

static int Boot_VectorsValid(uint32_t size)
{
  const uint32_t *vectors = (const uint32_t*)BOOT_APP_START;
  uint32_t reset = vectors[1] & ~1u;
  return size >= 8 && size <= BOOT_APP_MAX &&
    vectors[0] > 0x20000000u && vectors[0] <= 0x2000C000u &&
    (vectors[0] & 7u) == 0 && (vectors[1] & 1u) &&
    reset >= BOOT_APP_START && reset < BOOT_APP_START + size;
}

int Boot_ApplicationValid(void)
{
  const BootMetadata *m = (const BootMetadata*)BOOT_META_ADDRESS;
  return m->magic == BOOT_MAGIC && m->inverted_length == ~m->length &&
    Boot_VectorsValid(m->length) &&
    Boot_CRC32((const void*)BOOT_APP_START, m->length) == m->crc;
}

__asm static void Boot_Jump(uint32_t stack, uint32_t entry)
{
  MSR MSP, r0
  MOVS r0, #0
  MSR CONTROL, r0
  ISB
  CPSIE I
  BX r1
}

void Boot_RunApplication(void)
{
  if (!Boot_ApplicationValid()) return;
  uint32_t sp = *(const uint32_t*)BOOT_APP_START;
  uint32_t entry = *(const uint32_t*)(BOOT_APP_START + 4);
  __disable_irq();
  /* RUN resets first, so this path is executed before USB/HSE are started. */
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;
  for (uint8_t i = 0; i < 8; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFFu;
    NVIC->ICPR[i] = 0xFFFFFFFFu;
  }
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
  SCB->SCR = 0;
  SCB->VTOR = BOOT_APP_START;
  __DSB(); __ISB();
  Boot_Jump(sp, entry);
}

static void Boot_Clock(void)
{
  RCC_OscInitTypeDef oscillator = {0};
  RCC_ClkInitTypeDef clocks = {0};
  RCC_PeriphCLKInitTypeDef peripheral = {0};
  oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  oscillator.HSEState = RCC_HSE_ON;
  oscillator.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  oscillator.PLL.PLLState = RCC_PLL_ON;
  oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  oscillator.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) Error_Handler();
  clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clocks.APB1CLKDivider = RCC_HCLK_DIV2;
  clocks.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
  peripheral.PeriphClockSelection = RCC_PERIPHCLK_USB;
  peripheral.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&peripheral) != HAL_OK) Error_Handler();
}

int main(void)
{
  HAL_Init();
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_RCC_BKP_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();
  uint8_t requested = BKP->DR10 == BOOT_REQUEST;
  BKP->DR10 = 0;
  __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIO_InitTypeDef pin = {0};
  pin.Pin = GPIO_PIN_9; /* Center button, active high. */
  pin.Mode = GPIO_MODE_INPUT;
  pin.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &pin);
  HAL_Delay(25);
  if (!requested && HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9) == GPIO_PIN_RESET)
    Boot_RunApplication();
  Boot_Clock();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  pin.Pin = USB_CTRL_Pin;
  pin.Mode = GPIO_MODE_OUTPUT_PP;
  pin.Pull = GPIO_NOPULL;
  pin.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_WritePin(GPIOA, USB_CTRL_Pin, GPIO_PIN_RESET);
  HAL_GPIO_Init(GPIOA, &pin);
  pin.Pin = GPIO_PIN_4 | GPIO_PIN_6;
  HAL_GPIO_Init(GPIOC, &pin);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET); /* LCD backlight off. */
  HAL_Delay(250); /* Physical USB detach before enumeration. */
  MX_USB_DEVICE_Init();
  for (;;)
  {
    Boot_ProcessUSB();
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4,
      (HAL_GetTick() / 250) & 1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
}

void SysTick_Handler(void) { HAL_IncTick(); }
void USB_LP_CAN1_RX0_IRQHandler(void) { HAL_PCD_IRQHandler(&hpcd_USB_FS); }
void Error_Handler(void) { __disable_irq(); for (;;) {} }
