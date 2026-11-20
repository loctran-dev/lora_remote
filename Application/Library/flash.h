#ifndef FLASH_H
#define FLASH_H
#include "stm32f1xx_hal.h"

#define CONFIG_DATA_ADDRESS 0x0803F000
uint32_t writeDataToFlash(uint32_t address, uint32_t *data, uint16_t length);
//memcpy(&ex_struct, (uint32_t *)address, sizeof(ex_struct));
#endif
