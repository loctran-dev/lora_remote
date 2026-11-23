#include "flash.h"
#include <string.h>

uint32_t Flash_WriteData(uint32_t address, uint32_t *data, uint16_t length)
{
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError;
    uint32_t word;
    uint16_t remaining;

    if (length == 0)
    {
        return 0;
    }

    /* Erase every page covered by the data. */
    HAL_FLASH_Unlock();
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = address;
    EraseInitStruct.NbPages = (length + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return HAL_FLASH_GetError();
    }

    /* Callers pass a byte count, not a count of uint32_t words. */
    for (uint32_t i = 0; i < length; i += 4)
    {
        word = 0xFFFFFFFFu;
        remaining = length - i;
        memcpy(&word, (uint8_t *)data + i, remaining < 4 ? remaining : 4);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, word) == HAL_OK)
        {
            address += 4;
        }
        else
        {
            HAL_FLASH_Lock();
            return HAL_FLASH_GetError();
        }
    }

    HAL_FLASH_Lock();
    return 0;
}
