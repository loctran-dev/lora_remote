#include "config_store.h"
#include "utility.h"
#include "lora.h"
#include <stddef.h>
#include <string.h>

#define CONFIG_MAGIC 0x33474643u /* "CFG3": SystemStorageData_t with the MotorConfig pool */

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
static int8_t newestSlot(void);
static void loadDefaults(void);
static MotorStorageData_t *takeMotorSlot(uint8_t device, uint8_t module);
static void freeMotorSlot(MotorStorageData_t *motor);
static void sanitize(void);
static void sanitizeMotors(void);

/**
 * @brief  Fill storageData from flash.
 *
 * Takes the valid "CFG3" slot with the highest sequence number, else the
 * defaults, which are saved immediately. Blocks of older formats are not read.
 * Out-of-range settings and inconsistent motor slots are then corrected.
 *
 * @note   Call once from main() before the settings are used.
 */
void Config_Load(void)
{
    int8_t best = newestSlot();
    uint8_t useDefaults = 0;

    activeSlot = best;
    activeSequence = 0;
    if (best >= 0)
    {
        memcpy(&storageData, slotHeader(best) + 1, sizeof(storageData));
        activeSequence = slotHeader(best)->sequence;
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
 * @note   Called by systemTask when configSavePending is set; takes tens of ms.
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

/**
 * @brief  Settings slot of a motor.
 * @param  device  Index in DeviceConfig.
 * @param  module  Module index on that device.
 * @return The slot, or NULL if the motor has none (not paired, or the pool was full).
 */
MotorStorageData_t *Config_Motor(uint8_t device, uint8_t module)
{
    int8_t slot = Config_MotorSlot(device, module);

    return slot < 0 ? NULL : &storageData.MotorConfig[slot];
}

/**
 * @brief  Pool index of a motor; motorBuff uses the same index for its runtime state.
 * @param  device  Index in DeviceConfig.
 * @param  module  Module index on that device.
 * @return 0 .. MAXIMUM_MOTOR-1, or -1 if the motor has no slot.
 */
int8_t Config_MotorSlot(uint8_t device, uint8_t module)
{
    for (int8_t slot = 0; slot < MAXIMUM_MOTOR; slot++)
    {
        if (storageData.MotorConfig[slot].device == device && storageData.MotorConfig[slot].module == module)
        {
            return slot;
        }
    }
    return -1;
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
 * @note   Flash is written later by systemTask (configSavePending); the caller sets it.
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
        if (storageData.MotorConfig[slot].device == device && !storageData.MotorConfig[slot].hidden)
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
            storageData.MotorConfig[slot].hidden = hidden != FALSE;
        }
    }
}

static const Config_SlotHeader_t *slotHeader(uint8_t slot)
{
    return (const Config_SlotHeader_t *)(slot ? CONFIG_SLOT_B : CONFIG_SLOT_A);
}

/* A slot is valid when it has the CFG3 magic and length and the payload matches its CRC. */
static uint8_t slotValid(uint8_t slot)
{
    const Config_SlotHeader_t *header = slotHeader(slot);

    return header->magic == CONFIG_MAGIC && header->length == sizeof(SystemStorageData_t) &&
        header->crc == Util_Crc32(header + 1, header->length);
}

/* Valid slot with the highest sequence number (wrap-around safe), or -1. */
static int8_t newestSlot(void)
{
    int8_t best = -1;

    for (uint8_t slot = 0; slot < 2; slot++)
    {
        if (slotValid(slot) &&
            (best < 0 || (int32_t)(slotHeader(slot)->sequence - slotHeader(best)->sequence) > 0))
        {
            best = slot;
        }
    }
    return best;
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
        storageData.DeviceConfig[id].deleted = TRUE;
    }
    for (uint8_t slot = 0; slot < MAXIMUM_MOTOR; slot++)
    {
        freeMotorSlot(&storageData.MotorConfig[slot]);
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
        storageData.DeviceConfig[id].deleted = storageData.DeviceConfig[id].deleted != FALSE;
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
        if (motor->device >= MAXIMUM_DEVICE || storageData.DeviceConfig[motor->device].deleted ||
            motor->module >= MAXIMUM_MODULE || Config_Motor(motor->device, motor->module) != motor)
        {
            freeMotorSlot(motor);
            continue;
        }
        motor->locked = motor->locked == TRUE;
        motor->hidden = motor->hidden == TRUE;
        motor->name[sizeof(motor->name) - 1] = '\0';
        if (motor->name[0] == '\0')
        {
            strcpy(motor->name, "Default");
        }
    }
    for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
    {
        if (!storageData.DeviceConfig[id].deleted && Config_Motor(id, 0) == NULL)
        {
            takeMotorSlot(id, 0);
        }
    }
}
