/**
 * @file    define.h
 * @brief   Shared application types: device and system state, and the flash configuration block.
 *
 * deviceBuff[] and systemInfo (defined in main.c) hold the runtime state.
 * storageData (config_store.c) is the only copy of the settings; general
 * settings are read and edited there directly, device settings are copied
 * from deviceBuff by the ScrnCommon_Save* functions. The layout of the packed
 * storage structs is the flash format: append new fields at the end only.
 *
 * @defgroup define Shared types
 * @{
 */
#ifndef DEFINE_H
#define DEFINE_H
#include <stdint.h>
#include "stm32f1xx_hal.h"

#define MAXIMUM_DEVICE 20 ///< Paired device slots.
#define MAXIMUM_MODULE 6  ///< Motor modules per device.
#define MAXIMUM_MOTOR  40 ///< Motor settings slots shared by all devices (storageData.MotorConfig).
#define MOTOR_SLOT_FREE 0xFFu ///< MotorStorageData_t.device of an unused motor slot.

/** @name Battery icon steps [%] (not used; see ScrnCommon_UpdateBattery()) */
/** @{ */
#define BATTERY_LEVEL_1 5  ///< Not used.
#define BATTERY_LEVEL_2 25 ///< Not used.
#define BATTERY_LEVEL_3 50 ///< Not used.
#define BATTERY_LEVEL_4 75 ///< Not used.
/** @} */

/** @name RSSI colour thresholds on device control, compared with DeviceInfo_t.rssi */
/** @{ */
#define RSSI_LEVEL_1 (115) ///< Above: tomato.
#define RSSI_LEVEL_2 (100) ///< Above: orange.
#define RSSI_LEVEL_3 (70)  ///< Above: yellow.
#define RSSI_LEVEL_4 (30)  ///< Above: green-yellow.
#define RSSI_LEVEL_5 (-10) ///< Above: light green; otherwise green.
/** @} */

/** @brief Device type reported in factory info. */
typedef enum Device_Type_e
{
    eDevice_Type_Motor = 0x01,    ///< Motor controller ('M').
    eDevice_Type_Pressure = 0x02, ///< Pressure sensor ('P').
    eDevice_Type_Encoder = 0x03,  ///< Encoder ('E').
} Device_Type_e;

/** @brief Motor power state; the values are also used on the radio. */
typedef enum Motor_PowerState_e
{
    eMotor_PowerState_Idle = 0x00, ///< Unknown or no change.
    eMotor_PowerState_On = 0x01,   ///< Running.
    eMotor_PowerState_Off = 0x02,  ///< Stopped.
} Motor_PowerState_e;

/** @brief Alarm code sent by a device in CMD_ALARM_MESSAGE. */
typedef enum Alarm_Name_e
{
    eAlarm_Name_None,                  ///< No alarm.
    eAlarm_Name_LostInputPower,        ///< Supply input lost.
    eAlarm_Name_LostMotorPower,        ///< Motor power lost.
    eAlarm_Name_DiffMotorCurrentPhase, ///< Phase currents differ.
} Alarm_Name_e;

/** @brief Buzzer pattern, see Main_SetBuzzerMode(). */
typedef enum Buzzer_Mode_e
{
    eBuzzer_Mode_Off = 0,    ///< Stop the buzzer.
    eBuzzer_Mode_ShortBeep,  ///< 100 ms: button feedback.
    eBuzzer_Mode_2ShortBeep, ///< Two short beeps: request accepted.
    eBuzzer_Mode_3ShortBeep, ///< Three short beeps: connection lost.
    eBuzzer_Mode_LongBeep,   ///< 300 ms: request rejected or failed.
    eBuzzer_Mode_AlarmLong   ///< Continuous until eBuzzer_Mode_Off.
} Buzzer_Mode_e;

/** @brief Step of a pairing process (device search or connection). */
typedef enum Process_Step_e
{
    eProcess_Step_None = 0,   ///< Idle.
    eProcess_Step_Processing, ///< Request sent, waiting up to 5 s.
    eProcess_Step_Done,       ///< Reply received; systemTask picks up the result.
} Process_Step_e;

/** @brief Result of a pairing process. */
typedef enum Process_Result_e
{
    eProcess_Result_Passed = 0, ///< Succeeded.
    eProcess_Result_Failed,     ///< Failed or not finished.
} Process_Result_e;

/** @brief Power mode of the remote, see power_sleep.h. */
typedef enum System_RunMode_e
{
    eSystem_RunMode_DeepSleep = 0, ///< Not used.
    eSystem_RunMode_Sleep,         ///< Backlight off; STOP whenever idle.
    eSystem_RunMode_Normal,        ///< Display on.
    eSystem_RunMode_Alarm,         ///< Display on for a device error; no sleep until a user action.
} System_RunMode_e;

/** @brief Identity of a device, read with CMD_GET_FACTORY_INFO. */
typedef struct
{
    uint32_t serial;     ///< Serial number.
    uint8_t type;        ///< A Device_Type_e value.
    char typeChar;       ///< Letter shown in device names ('M', 'P', 'E', 'x').
    uint8_t moduleCount; ///< Motor modules on the device.
} FactoryInfo_t;

/**
 * @brief Runtime state of one motor, an entry of motorBuff.
 *
 * motorBuff[i] belongs to the motor that owns storageData.MotorConfig[i]
 * (device, module); find it with Main_MotorState().
 */
typedef struct
{
    uint8_t alarmReported;  ///< An alarm was reported.
    Alarm_Name_e alarmCode; ///< Code of the last alarm.
    uint32_t alarmDevTime;  ///< Device Unix time of the last alarm (device clock, not the remote's).

    Motor_PowerState_e pwrState; ///< Last reported power state.
    float phaseCurrent[3];       ///< Last reported phase currents [A].
    uint32_t autoOffTime;        ///< Unix time the motor switches itself off; 0 = no timer.
} MotorData_t;

/**
 * @brief Runtime state of one device slot.
 *
 * Identity lives in storageData.DeviceConfig[] at the same index; each motor
 * has its settings in storageData.MotorConfig and its state in motorBuff.
 */
typedef struct
{
    uint8_t index;    ///< Own index in deviceBuff.
    uint32_t devTime; ///< Device Unix time from the last status (device clock).
    int rssi;         ///< RSSI of the last packet from the device [dBm].

    uint32_t lastRxTick; ///< RTOS tick of the last packet from the device [ms].
    uint32_t lastRxTime; ///< Remote Unix time of the last packet (also set at boot and pairing).
    uint8_t linkLost;    ///< Set by the link monitor when probes fail.
} DeviceInfo_t;

/** @brief Runtime state of the remote. */
typedef struct
{
    uint8_t netId; ///< Network id the radio uses now (the default while pairing).

    uint8_t batteryPercent;  ///< Battery charge [%].
    uint32_t batteryAdc;     ///< Raw ADC sample, written by DMA.
    float batteryVoltage;    ///< Filtered battery voltage [V]; 0 = no valid reading.
    float batteryVoltageMin; ///< Voltage of an empty battery [V].
    float batteryVoltageMax; ///< Voltage of a full battery [V].
    RTC_TimeTypeDef rtcTime; ///< RTC time, read every systemTask loop.
    RTC_DateTypeDef rtcDate; ///< RTC date, read every systemTask loop.
    uint32_t currentTime;    ///< Unix time of rtcTime/rtcDate (remote clock).
    uint8_t charging;        ///< Charger reports charging.
    uint8_t batteryFull;     ///< Charger reports full (standby).

    /* Pairing: device search, then connection */
    Process_Step_e searchStep;      ///< Device search step.
    Process_Result_e searchResult;  ///< Device search result.
    uint32_t searchTick;            ///< RTOS tick when the search started [ms].
    Process_Step_e connectStep;     ///< Connection step.
    Process_Result_e connectResult; ///< Connection result.
    uint32_t connectTick;           ///< RTOS tick when the connection started [ms].
    uint8_t pairNodeId;             ///< Node id offered to the device being paired.
    FactoryInfo_t foundDevice;      ///< Identity of the device found by the search.

    uint8_t curDevice; ///< Selected device (index in deviceBuff).
    uint8_t curModule; ///< Selected module on that device.

    uint8_t configSavePending; ///< storageData changed; systemTask writes it to flash.
    uint32_t lastActionTick;   ///< RTOS tick of the last user action [ms].
    System_RunMode_e runMode;  ///< Power mode.
} SystemInfo_t;

/** @brief Stored identity of one device slot (flash format); its motors are in MotorConfig. */
#pragma pack(1)
typedef struct DeviceStorageData_t
{
    uint8_t nodeId;            ///< LoRa node id.
    FactoryInfo_t factoryInfo; ///< Identity from pairing; moduleCount motors are allocated.
    uint8_t deleted;           ///< Slot is free.
} DeviceStorageData_t;
#pragma pack()

/**
 * @brief Stored settings of one motor (flash format), a slot of the shared MotorConfig pool.
 *
 * A slot belongs to the motor (device, module); device == MOTOR_SLOT_FREE marks
 * an unused slot. Use Config_Motor() to find the slot of a motor.
 */
#pragma pack(1)
typedef struct MotorStorageData_t
{
    uint8_t device;          ///< Owner index in DeviceConfig, or MOTOR_SLOT_FREE.
    uint8_t module;          ///< Module index on that device, 0 .. MAXIMUM_MODULE-1.
    char name[20];           ///< Display name.
    float alarmThreshold;    ///< Low-current alarm threshold [A], 0 .. 99.99.
    uint32_t alarmDelay;     ///< Time below the threshold before the alarm [s].
    uint8_t locked;          ///< Power commands are refused.
    uint8_t hidden;          ///< Hidden from device control.
} MotorStorageData_t;
#pragma pack()

/** @brief Configuration block stored in a config slot (flash format "CFG3", see config_store.h). */
#pragma pack(1)
typedef struct SystemStorageData_t
{
    uint8_t netIdConfig; ///< Network id.
    int frequencyConfig; ///< Radio frequency [MHz].
    uint8_t sfConfig;    ///< LoRa spreading factor.

    uint8_t sleepMode;     ///< 1 = sleep allowed.
    uint32_t tUpdate;      ///< Link scan period outside SLEEP [s].
    uint32_t tSleepUpdate; ///< RTC wake-up period in SLEEP [s].

    DeviceStorageData_t DeviceConfig[MAXIMUM_DEVICE]; ///< Device slots.
    MotorStorageData_t MotorConfig[MAXIMUM_MOTOR];    ///< Motor settings pool shared by all devices.

    uint8_t lastWriteSuccess; ///< Always TRUE when saved; no longer read, kept so the CFG3 layout stays put.
    /* Link settings; covered by linkSettingsChecksum, change them through Link_Set...(). */
    uint32_t linkSettingsMagic;                 ///< Marks the link settings as present.
    uint16_t sleepAfterIdleSeconds;             ///< Idle time before SLEEP [s].
    uint16_t connectionTimeoutSeconds;          ///< Silence before a device is probed [s].
    uint16_t autoStatusSeconds[MAXIMUM_DEVICE]; ///< Auto-status period per device [s].
    uint8_t linkSettingsChecksum;               ///< Util_CalculateCRC() of the link settings.
} SystemStorageData_t;
#pragma pack()

#endif
/** @} */
