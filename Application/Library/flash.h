/**
 * @file    flash.h
 * @brief   Erasing and programming internal flash.
 *
 * @defgroup flash Flash
 * @{
 */
#ifndef FLASH_H
#define FLASH_H
#include "stm32f1xx_hal.h"

/** @brief Start of the 4 KiB configuration area (two 2 KiB pages, see config_store.h). */
#define CONFIG_DATA_ADDRESS 0x0803F000

uint32_t Flash_Erase(uint32_t address, uint32_t length);
uint32_t Flash_Program(uint32_t address, const uint32_t *data, uint16_t length);
uint32_t Flash_WriteData(uint32_t address, uint32_t *data, uint16_t length);

#endif
/** @} */
