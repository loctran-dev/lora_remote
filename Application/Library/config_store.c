#include "config_store.h"
#include "utility.h"
#include "lora.h"
#include <stddef.h>
#include <string.h>

#define CONFIG_MAGIC    0x33474643u /* "CFG3": SystemStorageData_t with the MotorConfig pool */
#define CONFIG_MAGIC_V2 0x32474643u /* "CFG2": ConfigV2_t, one set of settings per device */

/* Header at the start of each slot; the payload follows it. */
typedef struct
{
    uint32_t sequence;
    uint16_t length;
    uint16_t reserved;
    uint32_t crc;
    uint32_t magic;
} Config_SlotHeader_t;

/* Flash format before the motor pool ("CFG2" slots and the older raw copy at slot A).
   Kept only to convert old boards; do not change. */
#pragma pack(1)
typedef struct
{
    uint8_t nodeIdConfig;
    uint8_t moduleIndex;
    FactoryInfo_t factoryInfo;
    char name[20];
    float alarmThreshold;
    uint32_t alarmDelayTime;
    uint8_t isLock;
    uint8_t isHide;
    uint8_t isDelete;
} ConfigV2Device_t;

typedef struct
{
    uint8_t netIdConfig;
    int frequencyConfig;
    uint8_t sfConfig;
    uint8_t sleepMode;
    uint32_t tUpdate;
    uint32_t tSleepUpdate;
    ConfigV2Device_t DeviceConfig[MAXIMUM_DEVICE];
    uint8_t lastWriteSuccess;
    uint32_t linkSettingsMagic;
    uint16_t sleepAfterIdleSeconds;
    uint16_t connectionTimeoutSeconds;
    uint16_t autoStatusSeconds[MAXIMUM_DEVICE];
    uint8_t linkSettingsChecksum;
} ConfigV2_t;
#pragma pack()

typedef char Config_CheckSlotSize_t[(sizeof(Config_SlotHeader_t) + sizeof(SystemStorageData_t) <= CONFIG_SLOT_SIZE) ? 1 : -1];
typedef char Config_CheckV2Size_t[(sizeof(ConfigV2_t) == 885) ? 1 : -1];

SystemStorageData_t storageData;

static int8_t activeSlot = -1;
static uint32_t activeSequence;

static const Config_SlotHeader_t *slotHeader(uint8_t slot);
static uint8_t slotValid(uint8_t slot, uint32_t magic, uint16_t length);
static int8_t newestSlot(uint32_t magic, uint16_t length);
static uint8_t legacyValid(void);
static void loadDefaults(void);
static void convertV2(const ConfigV2_t *old);
static MotorStorageData_t *takeMotorSlot(uint8_t device, uint8_t module);
static void freeMotorSlot(MotorStorageData_t *motor);
static void sanitize(void);
static void sanitizeMotors(void);

/**
 * @brief  Fill storageData from flash.
 *
 * Takes the valid "CFG3" slot with the highest sequence number. Otherwise an
 * older format is converted: the newest "CFG2" slot, else the raw single-copy
 * format at slot A. Otherwise the defaults are loaded and saved immediately.
 * Out-of-range settings and inconsistent motor slots are then corrected.
 *
 * A converted block is written by the next save, to the other slot, so the old
 * copy stays readable until the new one is committed.
 *
 * @note   Call once from main() before the settings are used.
 */
void Config_Load(void)
{
    int8_t best = newestSlot(CONFIG_MAGIC, sizeof(SystemStorageData_t));
    uint8_t useDefaults = 0;

    activeSlot = -1;
    activeSequence = 0;
    if (best >= 0)
    {
        memcpy(&storageData, slotHeader(best) + 1, sizeof(storageData));
        activeSlot = best;
        activeSequence = slotHeader(best)->sequence;
    }
    else if ((best = newestSlot(CONFIG_MAGIC_V2, sizeof(ConfigV2_t))) >= 0)
    {
        convertV2((const ConfigV2_t *)(slotHeader(best) + 1));
        activeSlot = best;
        activeSequence = slotHeader(best)->sequence;
    }
    else if (legacyValid())
    {
        /* Raw format in slot A: the first save goes to slot B and keeps it. */
        convertV2((const ConfigV2_t *)CONFIG_SLOT_A);
        activeSlot = 0;
    }
    else
    {
        loadDefaults();
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
    if (!slotValid(target, CONFIG_MAGIC, sizeof(storageData)) ||
        memcmp(slotHeader(target) + 1, &storageData, sizeof(storageData)))
    {
        return CONFIG_ERROR_VERIFY;
    }

    activeSlot = target;
    activeSequence = header.sequence;
    return 0;
}

/**
 * @brief  Settings slot of a motor.
 * @param  device  Index in DeviceConfig.
 * @param  module  Module index on that device.
 * @return The slot, or NULL if the motor has none (not paired, or the pool was full).
 */
MotorStorageData_t *Config_Motor(uint8_t device, uint8_t module)
{
    for (uint8_t slot = 0; slot < MAXIMUM_MOTOR; slot++)
    {
        if (storageData.MotorConfig[slot].device == device && storageData.MotorConfig[slot].module == module)
        {
            return &storageData.MotorConfig[slot];
        }
    }
    return NULL;
}

/**
 * @brief  Number of motors to allocate for a device reporting numberOfModule modules.
 * @return numberOfModule limited to 1 .. MAXIMUM_MODULE.
 */
uint8_t Config_ModuleCount(uint8_t numberOfModule)
{
    if (numberOfModule == 0)
    {
        return 1;
    }
    return numberOfModule > MAXIMUM_MODULE ? MAXIMUM_MODULE : numberOfModule;
}

/**
 * @brief  Whether a device could get count motor slots, reusing the slots it holds.
 * @param  device  Index in DeviceConfig.
 * @param  count   Motors needed.
 */
uint8_t Config_CanAllocateMotors(uint8_t device, uint8_t count)
{
    uint8_t available = 0;

    for (uint8_t slot = 0; slot < MAXIMUM_MOTOR; slot++)
    {
        if (storageData.MotorConfig[slot].device == MOTOR_SLOT_FREE ||
            storageData.MotorConfig[slot].device == device)
        {
            available++;
        }
    }
    return available >= count;
}

/**
 * @brief  Give a device count motor slots (modules 0 .. count-1) with the default settings.
 *
 * The slots the device held before are released first. Nothing changes if the
 * pool cannot hold count motors.
 *
 * @param  device  Index in DeviceConfig.
 * @param  count   Motors, 1 .. MAXIMUM_MODULE.
 * @return TRUE on success, FALSE if the pool is too full.
 * @note   Flash is written later by systemTask (isNeedSaveConfig); the caller sets it.
 */
uint8_t Config_AllocateMotors(uint8_t device, uint8_t count)
{
    if (device >= MAXIMUM_DEVICE || count > MAXIMUM_MODULE || !Config_CanAllocateMotors(device, count))
    {
        return FALSE;
    }
    Config_FreeMotors(device);
    for (uint8_t module = 0; module < count; module++)
    {
        takeMotorSlot(device, module);
    }
    return TRUE;
}

/**
 * @brief  Release every motor slot of a device (unpairing).
 * @param  device  Index in DeviceConfig.
 */
void Config_FreeMotors(uint8_t device)
{
    for (uint8_t slot = 0; slot < MAXIMUM_MOTOR; slot++)
    {
        if (storageData.MotorConfig[slot].device == device)
        {
            freeMotorSlot(&storageData.MotorConfig[slot]);
        }
    }
}

/**
 * @brief  Whether a device has no visible motor (all hidden, or none allocated).
 * @param  device  Index in DeviceConfig.
 */
uint8_t Config_DeviceHidden(uint8_t device)
{
    for (uint8_t slot = 0; slot < MAXIMUM_MOTOR; slot++)
    {
        if (storageData.MotorConfig[slot].device == device && !storageData.MotorConfig[slot].isHide)
        {
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * @brief  Hide or show every motor of a device (device manager).
 * @param  device  Index in DeviceConfig.
 * @param  hidden  TRUE to hide.
 */
void Config_SetDeviceHidden(uint8_t device, uint8_t hidden)
{
    for (uint8_t slot = 0; slot < MAXIMUM_MOTOR; slot++)
    {
        if (storageData.MotorConfig[slot].device == device)
        {
            storageData.MotorConfig[slot].isHide = hidden != FALSE;
        }
    }
}

static const Config_SlotHeader_t *slotHeader(uint8_t slot)
{
    return (const Config_SlotHeader_t *)(slot ? CONFIG_SLOT_B : CONFIG_SLOT_A);
}

/* A slot is valid when it has the given magic and length and the payload matches its CRC. */
static uint8_t slotValid(uint8_t slot, uint32_t magic, uint16_t length)
{
    const Config_SlotHeader_t *header = slotHeader(slot);

    return header->magic == magic && header->length == length &&
        header->crc == Util_Crc32(header + 1, header->length);
}

/* Valid slot of one format with the highest sequence number (wrap-around safe), or -1. */
static int8_t newestSlot(uint32_t magic, uint16_t length)
{
    int8_t best = -1;

    for (uint8_t slot = 0; slot < 2; slot++)
    {
        if (slotValid(slot, magic, length) &&
            (best < 0 || (int32_t)(slotHeader(slot)->sequence - slotHeader(best)->sequence) > 0))
        {
            best = slot;
        }
    }
    return best;
}

static uint8_t legacyValid(void)
{
    return slotHeader(0)->magic != CONFIG_MAGIC && slotHeader(0)->magic != CONFIG_MAGIC_V2 &&
        ((const ConfigV2_t *)CONFIG_SLOT_A)->lastWriteSuccess == TRUE;
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
    for (uint8_t slot = 0; slot < MAXIMUM_MOTOR; slot++)
    {
        freeMotorSlot(&storageData.MotorConfig[slot]);
    }
}

/*
 * Convert a block in the old one-settings-per-device format. Module 0 of each
 * paired device takes over its name, alarm, lock and hide; the other modules it
 * reports get the defaults, as far as the pool allows (module 0 of every device
 * is placed first). The link settings are copied unchanged, so their checksum
 * stays valid.
 */
static void convertV2(const ConfigV2_t *old)
{
    MotorStorageData_t *motor;

    loadDefaults();
    storageData.netIdConfig = old->netIdConfig;
    storageData.frequencyConfig = old->frequencyConfig;
    storageData.sfConfig = old->sfConfig;
    storageData.sleepMode = old->sleepMode;
    storageData.tUpdate = old->tUpdate;
    storageData.tSleepUpdate = old->tSleepUpdate;
    storageData.linkSettingsMagic = old->linkSettingsMagic;
    storageData.sleepAfterIdleSeconds = old->sleepAfterIdleSeconds;
    storageData.connectionTimeoutSeconds = old->connectionTimeoutSeconds;
    memcpy(storageData.autoStatusSeconds, old->autoStatusSeconds, sizeof(storageData.autoStatusSeconds));
    storageData.linkSettingsChecksum = old->linkSettingsChecksum;

    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        storageData.DeviceConfig[id].nodeIdConfig = old->DeviceConfig[id].nodeIdConfig;
        storageData.DeviceConfig[id].factoryInfo = old->DeviceConfig[id].factoryInfo;
        storageData.DeviceConfig[id].isDelete = old->DeviceConfig[id].isDelete != FALSE;
        if (storageData.DeviceConfig[id].isDelete || (motor = takeMotorSlot(id, 0)) == NULL)
        {
            continue;
        }
        memcpy(motor->name, old->DeviceConfig[id].name, sizeof(motor->name));
        motor->alarmThreshold = old->DeviceConfig[id].alarmThreshold;
        motor->alarmDelayTime = old->DeviceConfig[id].alarmDelayTime;
        motor->isLock = old->DeviceConfig[id].isLock;
        motor->isHide = old->DeviceConfig[id].isHide;
    }
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        for (uint8_t module = 1;
             !storageData.DeviceConfig[id].isDelete &&
             module < Config_ModuleCount(storageData.DeviceConfig[id].factoryInfo.numberOfModule);
             module++)
        {
            takeMotorSlot(id, module);
        }
    }
}

/* Claim a free slot for (device, module) with the default settings; NULL if the pool is full. */
static MotorStorageData_t *takeMotorSlot(uint8_t device, uint8_t module)
{
    for (uint8_t slot = 0; slot < MAXIMUM_MOTOR; slot++)
    {
        MotorStorageData_t *motor = &storageData.MotorConfig[slot];

        if (motor->device == MOTOR_SLOT_FREE)
        {
            memset(motor, 0, sizeof(*motor));
            motor->device = device;
            motor->module = module;
            strcpy(motor->name, "Default");
            return motor;
        }
    }
    return NULL;
}

static void freeMotorSlot(MotorStorageData_t *motor)
{
    memset(motor, 0, sizeof(*motor));
    motor->device = MOTOR_SLOT_FREE;
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
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        storageData.DeviceConfig[id].isDelete = storageData.DeviceConfig[id].isDelete != FALSE;
    }
    sanitizeMotors();
}

/*
 * Keep the pool consistent: release slots of unknown or deleted devices, bad
 * module numbers and duplicates; normalise flags and names; give a paired
 * device without a slot for module 0 one with the defaults if the pool allows.
 */
static void sanitizeMotors(void)
{
    for (uint8_t slot = 0; slot < MAXIMUM_MOTOR; slot++)
    {
        MotorStorageData_t *motor = &storageData.MotorConfig[slot];

        if (motor->device == MOTOR_SLOT_FREE)
        {
            continue;
        }
        if (motor->device >= MAXIMUM_DEVICE || storageData.DeviceConfig[motor->device].isDelete ||
            motor->module >= MAXIMUM_MODULE || Config_Motor(motor->device, motor->module) != motor)
        {
            freeMotorSlot(motor);
            continue;
        }
        motor->isLock = motor->isLock == TRUE;
        motor->isHide = motor->isHide == TRUE;
        motor->name[sizeof(motor->name) - 1] = '\0';
        if (motor->name[0] == '\0')
        {
            strcpy(motor->name, "Default");
        }
    }
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        if (!storageData.DeviceConfig[id].isDelete && Config_Motor(id, 0) == NULL)
        {
            takeMotorSlot(id, 0);
        }
    }
}
