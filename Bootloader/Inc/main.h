#ifndef BOOT_MAIN_H
#define BOOT_MAIN_H
#include "stm32f1xx_hal.h"
#define USB_CTRL_Pin GPIO_PIN_10
#define USB_CTRL_GPIO_Port GPIOA
void Error_Handler(void);
#endif
