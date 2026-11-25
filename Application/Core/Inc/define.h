#ifndef DEFINE_H
#define DEFINE_H
#include <stdint.h>
#include "stm32f1xx_hal.h"

#define MAXIMUM_DEVICE 20

#define BATTERY_LEVEL_1 5
#define BATTERY_LEVEL_2 25
#define BATTERY_LEVEL_3 50
#define BATTERY_LEVEL_4 75

#define RSSI_LEVEL_1 (115)
#define RSSI_LEVEL_2 (100)
#define RSSI_LEVEL_3 (70)
#define RSSI_LEVEL_4 (30)
#define RSSI_LEVEL_5 (-10)

typedef enum Device_Type_e
{
    eDevice_Type_Motor = 0x01,
    eDevice_Type_Pressure = 0x02,
    eDevice_Type_Encoder = 0x03,
} Device_Type_e;

typedef enum Motor_PowerState_e
{
    eMotor_PowerState_Idle = 0x00,
    eMotor_PowerState_On = 0x01,
    eMotor_PowerState_Off = 0x02,
} Motor_PowerState_e;

typedef enum Alarm_Name_e
{
    eAlarm_Name_None,
    eAlarm_Name_LostInputPower,
    eAlarm_Name_LostMotorPower,
    eAlarm_Name_DiffMotorCurrentPhase,
} Alarm_Name_e;

typedef enum Buzzer_Mode_e
{
    eBuzzer_Mode_Off = 0,
    eBuzzer_Mode_ShortBeep,
    eBuzzer_Mode_2ShortBeep,
    eBuzzer_Mode_3ShortBeep,
    eBuzzer_Mode_LongBeep,
    eBuzzer_Mode_AlarmLong
} Buzzer_Mode_e;

typedef enum Process_Step_e
{
    eProcess_Step_None = 0,
    eProcess_Step_Processing,
    eProcess_Step_Done,
} Process_Step_e;

typedef enum Process_Result_e
{
    eProcess_Result_Passed = 0,
    eProcess_Result_Failed,
} Process_Result_e;

typedef enum System_RunMode_e
{
    eSystem_RunMode_DeepSleep = 0,
    eSystem_RunMode_Sleep,
    eSystem_RunMode_Normal,
    eSystem_RunMode_Alarm,
} System_RunMode_e;

typedef struct
{
    uint32_t serial;
    uint8_t deviceType;
    char deviceTypeChar;
    uint8_t numberOfModule;
} FactoryInfo_t;

typedef struct
{
    uint8_t moduleIndex;

    uint8_t isAlarmOccurred;
    Alarm_Name_e alarm;
    uint32_t alarmTime;

    Motor_PowerState_e pwrState;
    float phaseCurrent[3];
    uint16_t countdownMinutes;
    uint32_t startTime;
    uint32_t stopTime;
    uint32_t scheduleTime;

    /* Settings */
    char name[20];
    float alarmThreshold;
    uint32_t alarmDelayTime;
    uint8_t isLock;
    uint8_t isHide;
} MotorData_t;

typedef struct
{
    uint8_t deviceIndex;
    uint8_t nodeId;
    FactoryInfo_t factoryInfo;
    uint32_t localTime;
    int rssi;

    uint8_t isDelete;

    uint32_t lastCommTick;
    uint32_t lastCommTime;
    uint8_t isConnectionError;

    MotorData_t motorData[6];
} DeviceInfo_t;

typedef struct
{
    uint8_t netId;
    uint8_t netIdConfig;
    int frequencyConfig;
    uint8_t sfConfig;

    uint8_t batteryUsage;
    uint32_t batteryAdc;
    float batteryVoltage;
    float batteryVoltageMin;
    float batteryVoltageMax;
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    uint32_t currentTime;
    uint8_t isChanging;
    uint8_t isFullBattery;

    uint8_t sleepMode;
    uint32_t tUpdate;
    uint32_t tSleepUpdate;

    /* New connection process */
    Process_Step_e findDeviceStep;       /* Idle, Processing, Done */
    Process_Result_e findDeviceResult;   /* Passed, Failed */
    uint32_t findDeviceTick;
    Process_Step_e configDeviceStep;     /* Idle, Processing, Done */
    Process_Result_e configDeviceResult; /* Passed, Failed */
    uint32_t configDeviceTick;
    uint8_t nextNodeId;
    FactoryInfo_t findDevicefactoryInfo;

    uint8_t currentDeviceId;
    uint8_t currentModuleId;

    uint8_t isNeedSaveConfig;
    uint32_t lastActionTick;
    uint32_t lastReceiveMsgTick;
    System_RunMode_e runMode;
} SystemInfo_t;

#pragma pack(1)
typedef struct DeviceStorageData_t
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
} DeviceStorageData_t;
#pragma pack()

#pragma pack(1)
typedef struct SystemStorageData_t
{
    uint8_t netIdConfig;
    int frequencyConfig;
    uint8_t sfConfig;

    uint8_t sleepMode;
    uint32_t tUpdate;
    uint32_t tSleepUpdate;

    DeviceStorageData_t DeviceConfig[MAXIMUM_DEVICE];

    uint8_t lastWriteSuccess;
    /* Appended extension; legacy settings and marker offsets stay unchanged. */
    uint32_t linkSettingsMagic;
    uint16_t sleepAfterIdleSeconds;
    uint16_t connectionTimeoutSeconds;
    uint16_t autoStatusSeconds[MAXIMUM_DEVICE];
    uint8_t linkSettingsChecksum;
} SystemStorageData_t;
#pragma pack()

#endif
