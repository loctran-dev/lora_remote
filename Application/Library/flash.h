/**
 * @file    flash.h
 * @brief   Writing the configuration block to internal flash.
 *
 * @defgroup flash Flash
 * @{
 */
#ifndef FLASH_H
#define FLASH_H
#include "stm32f1xx_hal.h"

/** @brief Address of SystemStorageData_t (last 4 KiB of flash). */
#define CONFIG_DATA_ADDRESS 0x0803F000

uint32_t Flash_WriteData(uint32_t address, uint32_t *data, uint16_t length);

#endif
/** @} */
