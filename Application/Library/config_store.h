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
 * Boards still holding the old single-copy format (raw SystemStorageData_t at
 * CONFIG_DATA_ADDRESS, accepted when lastWriteSuccess is TRUE) are read once
 * and migrated by the first save, which goes to slot B.
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

#endif
/** @} */
