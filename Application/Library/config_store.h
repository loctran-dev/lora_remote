/**
 * @file    config_store.h
 * @brief   Power-loss-safe storage of the configuration block (SystemStorageData_t).
 *
 * The 4 KiB configuration area holds two 2 KiB slots, A at CONFIG_SLOT_A and B
 * at CONFIG_SLOT_B. Each save goes to the slot that does not hold the current
 * copy: the page is erased, the payload is programmed, then the header with a
 * sequence number and a CRC-32, its magic word last. A save cut short by a
 * power loss therefore leaves the previous slot untouched and valid; at
 * start-up the valid slot with the highest sequence number wins.
 *
 * Slots carry the magic "CFG3". Blocks of older formats are not read; a board
 * holding only those starts from the defaults.
 *
 * Motor settings live in the shared pool storageData.motors; a device gets
 * one slot per module when it is paired. Find a motor's slot with Config_Motor().
 *
 * @defgroup config Configuration storage
 * @{
 */
#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H
#include <stdint.h>
#include "define.h"
#include "flash.h"

#define CONFIG_SLOT_SIZE    0x800u                                   ///< One flash page [bytes].
#define CONFIG_SLOT_A       CONFIG_DATA_ADDRESS                      ///< First slot (also the old format).
#define CONFIG_SLOT_B       (CONFIG_DATA_ADDRESS + CONFIG_SLOT_SIZE) ///< Second slot.
#define CONFIG_ERROR_VERIFY 0x100u ///< Config_Save(): the read-back did not match.

/** @brief The settings in RAM; the only copy the application reads and edits. */
extern SystemStorageData_t storageData;

void Config_Load(void);
uint32_t Config_Save(void);

/* Motor settings pool */

MotorStorageData_t *Config_Motor(uint8_t device, uint8_t module);
int8_t Config_MotorSlot(uint8_t device, uint8_t module);
uint8_t Config_ModuleCount(uint8_t numberOfModule);
uint8_t Config_CanAllocateMotors(uint8_t device, uint8_t count);
uint8_t Config_AllocateMotors(uint8_t device, uint8_t count);
void Config_FreeMotors(uint8_t device);
uint8_t Config_DeviceHidden(uint8_t device);
void Config_SetDeviceHidden(uint8_t device, uint8_t hidden);

#endif
/** @} */
