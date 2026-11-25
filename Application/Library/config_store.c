#include "config_store.h"
#include "utility.h"
#include "lora.h"
#include <stddef.h>
#include <string.h>

#define CONFIG_MAGIC 0x32474643u /* "CFG2" */

/* Header at the start of each slot; the payload follows it. */
typedef struct
{
    uint32_t sequence;
    uint16_t length;
    uint16_t reserved;
    uint32_t crc;
    uint32_t magic;
} Config_SlotHeader_t;

typedef char Config_CheckSlotSize_t[(sizeof(Config_SlotHeader_t) + sizeof(SystemStorageData_t) <= CONFIG_SLOT_SIZE) ? 1 : -1];

SystemStorageData_t storageData;

static int8_t activeSlot = -1;
static uint32_t activeSequence;

static const Config_SlotHeader_t *slotHeader(uint8_t slot);
static uint8_t slotValid(uint8_t slot);
static uint8_t legacyValid(void);
static void loadDefaults(void);
static void sanitize(void);

/**
 * @brief  Fill storageData from flash.
 *
 * Takes the valid slot with the highest sequence number, else the old
 * single-copy format, else the defaults (saved immediately). Out-of-range
 * radio and period settings are then corrected.
 *
 * @note   Call once from main() before the settings are used.
 */
void Config_Load(void)
{
    int8_t best = -1;
    uint8_t useDefaults = 0;

    /* Newest valid slot. */
    for (uint8_t slot = 0; slot < 2; slot++)
    {
        if (slotValid(slot) &&
            (best < 0 || (int32_t)(slotHeader(slot)->sequence - slotHeader(best)->sequence) > 0))
        {
            best = slot;
        }
    }

    memset(&storageData, 0, sizeof(storageData));
    if (best >= 0)
    {
        memcpy(&storageData, slotHeader(best) + 1, slotHeader(best)->length);
        activeSlot = best;
        activeSequence = slotHeader(best)->sequence;
    }
    else if (legacyValid())
    {
        /* Old format in slot A: the first save goes to slot B and keeps it. */
        memcpy(&storageData, (const void *)CONFIG_SLOT_A, sizeof(storageData));
        activeSlot = 0;
        activeSequence = 0;
    }
    else
    {
        loadDefaults();
        activeSlot = -1;
        activeSequence = 0;
        useDefaults = 1;
    }

    sanitize();
    if (useDefaults)
    {
        Config_Save();
    }
}

/**
 * @brief  Write storageData to the slot that does not hold the current copy.
 * @return 0 on success, a HAL flash error code, or CONFIG_ERROR_VERIFY. On
 *         failure the previous copy is still the one loaded at start-up.
 * @note   Called by systemTask when isNeedSaveConfig is set; takes tens of ms.
 */
uint32_t Config_Save(void)
{
    Config_SlotHeader_t header;
    uint8_t target = activeSlot == 0 ? 1 : 0;
    uint32_t address = target ? CONFIG_SLOT_B : CONFIG_SLOT_A;
    uint32_t error;

    storageData.lastWriteSuccess = TRUE;
    header.sequence = activeSequence + 1;
    header.length = sizeof(storageData);
    header.reserved = 0xFFFFu;
    header.crc = Util_Crc32(&storageData, sizeof(storageData));
    header.magic = CONFIG_MAGIC;

    /* Payload first, then the header; the magic word makes the slot valid. */
    error = Flash_Erase(address, CONFIG_SLOT_SIZE);
    if (!error)
    {
        error = Flash_Program(address + sizeof(header), (const uint32_t *)&storageData, sizeof(storageData));
    }
    if (!error)
    {
        error = Flash_Program(address, (const uint32_t *)&header, offsetof(Config_SlotHeader_t, magic));
    }
    if (!error)
    {
        error = Flash_Program(address + offsetof(Config_SlotHeader_t, magic), &header.magic, sizeof(header.magic));
    }
    if (error)
    {
        return error;
    }
    if (!slotValid(target) || memcmp(slotHeader(target) + 1, &storageData, sizeof(storageData)))
    {
        return CONFIG_ERROR_VERIFY;
    }

    activeSlot = target;
    activeSequence = header.sequence;
    return 0;
}

static const Config_SlotHeader_t *slotHeader(uint8_t slot)
{
    return (const Config_SlotHeader_t *)(slot ? CONFIG_SLOT_B : CONFIG_SLOT_A);
}

/* A slot is valid when its magic is set and the payload matches its CRC. */
static uint8_t slotValid(uint8_t slot)
{
    const Config_SlotHeader_t *header = slotHeader(slot);

    return header->magic == CONFIG_MAGIC && header->length && header->length <= sizeof(SystemStorageData_t) &&
        header->crc == Util_Crc32(header + 1, header->length);
}

static uint8_t legacyValid(void)
{
    return slotHeader(0)->magic != CONFIG_MAGIC &&
        ((const SystemStorageData_t *)CONFIG_SLOT_A)->lastWriteSuccess == TRUE;
}

static void loadDefaults(void)
{
    memset(&storageData, 0, sizeof(storageData));
    storageData.netIdConfig = 0x01;
    storageData.frequencyConfig = 433;
    storageData.sfConfig = SF_7;
    storageData.lastWriteSuccess = TRUE;
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        storageData.DeviceConfig[id].isDelete = TRUE;
    }
}

static void sanitize(void)
{
    if (storageData.netIdConfig == 0 || storageData.netIdConfig == 255)
    {
        storageData.netIdConfig = 1;
    }
    if (storageData.frequencyConfig < 137 || storageData.frequencyConfig > 1020)
    {
        storageData.frequencyConfig = 433;
    }
    if (storageData.sfConfig < SF_7 || storageData.sfConfig > SF_12)
    {
        storageData.sfConfig = SF_7;
    }
    storageData.sleepMode = storageData.sleepMode == TRUE;
    if (storageData.tUpdate < 1 || storageData.tUpdate > 9999)
    {
        storageData.tUpdate = 5;
    }
    if (storageData.tSleepUpdate < 1 || storageData.tSleepUpdate > 9999)
    {
        storageData.tSleepUpdate = 300;
    }
}
