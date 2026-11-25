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

typedef enum
{
    WD_DEVICE_CTRL = 0x00,
    WD_GENERAL_SETTING,
    WD_NEW_CONNECTION,
} Window_t;

typedef enum
{
    WDT_MAIN = 0x00,

    WDT_DEV_SETTING,
    WDT_DEV_INFO,
    WDT_DEV_MESSAGE,
    WDT_DEV_SCHEDULE,

    WDT_NC_FINDING_DEVICE,
    WDT_NC_FOUND_FAILED,
    WDT_NC_FOUND_PASSED,
    WDT_NC_WAITING_CONFIG,
    WDT_NC_CONFIG_PASSED,
    WDT_NC_CONFIG_FAILED,
} WindowTab_t;

typedef enum
{
    DEV_TYPE_MOTOR = 0x01,
    DEV_TYPE_PRESSURE = 0x02,
    DEV_TYPE_ENCODER = 0x03,
} DeviceType_t;

typedef enum
{
    MOTOR_POWER_IDLE = 0x00,
    MOTOR_POWER_ON = 0x01,
    MOTOR_POWER_OFF = 0x02,
} MotorPowerState_t;

typedef enum
{
    CMD_STATE_LOST_CONNECTED,
    CMD_STATE_CONNECTING,
    CMD_STATE_SENDING,
    CMD_STATE_ERROR,
} CommandState_t;

typedef enum
{
    ALARM_NONE,
    ALARM_LOST_INPUT_POWER,
    ALARM_LOST_MOTOR_POWER,
    ALARM_DIFF_MOTOR_CURRENT_PHASE,
} AlarmName_t;

typedef enum
{
    BUZZER_OFF = 0,
    BUZZER_SHORT_BEEP,
    BUZZER_2_SHORT_BEEP,
    BUZZER_3_SHORT_BEEP,
    BUZZER_LONG_BEEP,
    BUZZER_ALARM_LONG
} BuzzerMode_t;

typedef enum
{
    STEP_NONE = 0,
    STEP_PROCESSING,
    STEP_DONE,
} ProcessStep_t;

typedef enum
{
    PROCESS_PASSED = 0,
    PROCESS_FAILED,
} ProcessResult_t;

typedef enum
{
    LCD_ON = 0,
    LCD_OFF,
} LCD_mode_t;

typedef enum
{
    SYS_MODE_DEEP_SLEEP = 0,
    SYS_MODE_SLEEP,
    SYS_MODE_NORMAL,
    SYS_MODE_ALARM,
} SystemRunMode_t;

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
    AlarmName_t alarm;
    uint32_t alarmTime;

    MotorPowerState_t pwrState;
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

    Window_t currentWindow;
    WindowTab_t currentTab;
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
    ProcessStep_t findDeviceStep;       /* Idle, Processing, Done */
    ProcessResult_t findDeviceResult;   /* Passed, Failed */
    uint32_t findDeviceTick;
    ProcessStep_t configDeviceStep;     /* Idle, Processing, Done */
    ProcessResult_t configDeviceResult; /* Passed, Failed */
    uint32_t configDeviceTick;
    uint8_t nextNodeId;
    FactoryInfo_t findDevicefactoryInfo;

    uint8_t currentDeviceId;
    uint8_t currentModuleId;

    uint8_t isNeedSaveConfig;
    LCD_mode_t lcdMode;
    uint32_t lastActionTick;
    uint32_t lastReceiveMsgTick;
    SystemRunMode_t runMode;
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
