/* Host shim for config_store tests: fake flash at the real addresses. */
#ifndef SHIM_HAL_H
#define SHIM_HAL_H
#include <stdint.h>
typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;
typedef struct { uint8_t Hours, Minutes, Seconds; } RTC_TimeTypeDef;
typedef struct { uint8_t WeekDay, Month, Date, Year; } RTC_DateTypeDef;
typedef struct { uint32_t TypeErase; uint32_t Banks; uint32_t PageAddress; uint32_t NbPages; } FLASH_EraseInitTypeDef;
#define FLASH_PAGE_SIZE 0x800u
#define FLASH_TYPEERASE_PAGES 0
#define FLASH_TYPEPROGRAM_WORD 2
HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *init, uint32_t *error);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uint32_t address, uint64_t data);
uint32_t HAL_FLASH_GetError(void);
#endif
