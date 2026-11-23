#ifndef FLASH_H
#define FLASH_H
#include "stm32f1xx_hal.h"

#define CONFIG_DATA_ADDRESS 0x0803F000

/* Erase the pages covering length bytes at address, then program data.
   Returns 0 on success, otherwise the HAL flash error code. */
uint32_t Flash_WriteData(uint32_t address, uint32_t *data, uint16_t length);

#endif
