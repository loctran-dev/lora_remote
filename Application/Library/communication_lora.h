/**
 * @file    communication_lora.h
 * @brief   LoRa frame format, payload layouts, frame builders and parsers.
 *
 * Every frame is: signature 0xFF55 (2 bytes), length, netId, nodeId, type,
 * transactionID (2 bytes), code, payload, checksum. `length` counts the bytes
 * from `length` up to the end of the payload; the checksum is the two's
 * complement of the byte sum over the first `length` bytes of the frame.
 * Multi-byte fields are little-endian. Received frames are validated by
 * Comm_ProcessRxData() and dispatched to the handlers in command_handlers.c.
 *
 * @defgroup comm LoRa communication
 * @{
 */
#ifndef COMMUNICATION_LORA_H
#define COMMUNICATION_LORA_H
#include "stdint.h"

/** @name Frame layout */
/** @{ */
#define LORA_PACKAGE_HEADER_SIZE        0x09 ///< Signature to code, in bytes.
#define LORA_PACKAGE_RAW_DATA_SIZE      256  ///< Largest frame, in bytes.
#define LORA_PACKAGE_CRC_SIZE           0x01 ///< Checksum size, in bytes.
#define LORA_PACKAGE_SERIAL_NUMBER_SIZE 0x0C ///< Serial number length, in bytes.
#define LORA_PACKAGE_TYPE_REQUEST       0x01 ///< `type` of a request.
#define LORA_PACKAGE_TYPE_RESPONSE      0x02 ///< `type` of a response.
/** @} */

/** @name Command codes */
/** @{ */
#define CMD_GET_STATUS        0x01 ///< Read power state, phase currents and schedule.
#define CMD_CONTROL_POWER     0x02 ///< Switch a motor on or off.
#define CMD_SET_SCHEDULE      0x03 ///< Set or cancel the switch-off time.
#define CMD_SYNC_TIME         0x04 ///< Set the device clock.
#define CMD_GET_FACTORY_INFO  0x05 ///< Read serial number and device type (pairing).
#define CMD_CONFIG_CONNECTION 0x06 ///< Give a device its network, node id and radio settings.
#define CMD_ALARM_MESSAGE     0x07 ///< Alarm sent by a device.
#define CMD_SET_CONFIG        0x08 ///< Set alarm threshold/delay and auto-status period.
/** @} */

/** @name Addresses */
/** @{ */
#define LORA_NODE_ID_BROADCAST 0xFF ///< Node id that addresses every device.
#define LORA_NET_ID_DEFAULT    0x00 ///< Network id of an unpaired device.
#define LORA_NODE_ID_DEFAULT   0x00 ///< Node id of an unpaired device.
/** @} */

/** @brief Payload of CMD_SET_CONFIG. A single-motor device supports module 0 only. */
#pragma pack(1)
typedef struct
{
    uint8_t moduleIndex;             ///< Motor module, 0.
    uint16_t alarmThresholdCentiAmp; ///< Low-current alarm threshold [0.01 A].
    uint16_t alarmDelaySeconds;      ///< Time below the threshold before the alarm [s].
    uint16_t autoStatusSeconds;      ///< Period of unsolicited status reports [s].
} PayloadRequestSetConfig_t;
#pragma pack()

/** @brief One frame, decoded header plus the raw bytes as sent or received. */
#pragma pack(1)
typedef struct LoraPackage_t
{
    uint16_t signature;     ///< 0xFF55.
    uint8_t length;         ///< Bytes from `length` to the end of the payload.
    uint8_t netId;          ///< Network id.
    uint8_t nodeId;         ///< Node id of the device.
    uint8_t type;           ///< LORA_PACKAGE_TYPE_REQUEST or LORA_PACKAGE_TYPE_RESPONSE.
    uint16_t transactionID; ///< Matches a response to its request; 0 = unsolicited status.
    uint8_t code;           ///< Command code (CMD_...).
    uint8_t checkSum;       ///< Frame checksum.
    uint32_t byteCount;     ///< Frame size in rawData, checksum included [bytes].
    uint32_t timeout;       ///< Reply timeout requested by the builder [ms].
    uint8_t rawData[LORA_PACKAGE_RAW_DATA_SIZE]; ///< Complete frame.
} LoraPackage_t;
#pragma pack()

/* REQUEST PAYLOAD ========================================================== */

/** @brief Payload of a CMD_GET_STATUS request. */
#pragma pack(1)
typedef struct PayloadRequestGetStatus_t
{
    uint8_t moduleIndex; ///< Motor module, 0.
    uint32_t syncTime;   ///< Remote Unix time for the device clock; 0 = do not sync.
} PayloadRequestGetStatus_t;
#pragma pack()

/** @brief Payload of a CMD_CONTROL_POWER request. */
#pragma pack(1)
typedef struct PayloadRequestCtrlPower_t
{
    uint8_t moduleIndex; ///< Motor module, 0.
    uint8_t powerState;  ///< Requested state, a Motor_PowerState_e value.
} PayloadRequestCtrlPower_t;
#pragma pack()

/** @brief Payload of a CMD_SET_SCHEDULE request. */
#pragma pack(1)
typedef struct PayloadRequestSetSchedule_t
{
    uint8_t moduleIndex;   ///< Motor module, 0.
    uint32_t syncTime;     ///< Remote Unix time when the request was made.
    uint32_t scheduleTime; ///< Unix time of the switch-off; 0 = cancel the schedule.
} PayloadRequestSetSchedule_t;
#pragma pack()

/** @brief Payload of a CMD_SYNC_TIME request. */
#pragma pack(1)
typedef struct PayloadRequestSyncTime_t
{
    uint32_t unixTime; ///< New device time (Unix).
} PayloadRequestSyncTime_t;
#pragma pack()

/* Payload for CMD_GET_FACTORY_INFO: none. */

/** @brief Payload of a CMD_CONFIG_CONNECTION request (pairing). */
#pragma pack(1)
typedef struct PayloadRequestConfigConnection_t
{
    uint32_t serial;    ///< Serial number of the device being paired.
    uint8_t netId;      ///< Network id to use.
    uint8_t nodeId;     ///< Node id given to the device.
    uint32_t frequency; ///< Radio frequency [MHz].
    uint8_t sf;         ///< LoRa spreading factor.
} PayloadRequestConfigConnection_t;
#pragma pack()

/** @brief Payload of a CMD_ALARM_MESSAGE request sent by a device. */
#pragma pack(1)
typedef struct PayloadRequestAlarm_t
{
    uint8_t moduleIndex; ///< Motor module, 0.
    uint8_t alarmCode;   ///< An Alarm_Name_e value.
    uint32_t alarmTime;  ///< Device Unix time of the alarm.
} PayloadRequestAlarm_t;
#pragma pack()

/* RESPONSE PAYLOAD ========================================================= */

/** @brief Response payload of CMD_CONTROL_POWER, CMD_SET_SCHEDULE, CMD_SYNC_TIME and CMD_SET_CONFIG. */
#pragma pack(1)
typedef struct PayloadCommonResponse_t
{
    uint8_t moduleIndex; ///< Motor module, 0.
    uint8_t result;      ///< 1 = accepted, 0 = rejected.
} PayloadCommonResponse_t;
#pragma pack()

/** @brief Response payload of CMD_GET_STATUS. */
#pragma pack(1)
typedef struct PayloadResponseGetStatus_t
{
    uint8_t moduleIndex;      ///< Motor module, 0.
    uint8_t result;           ///< 1 = valid status.
    uint8_t powerState;       ///< A Motor_PowerState_e value.
    uint16_t phaseCurrent[3]; ///< Phase currents [0.01 A].
    uint32_t localTime;       ///< Device Unix time.
    uint32_t scheduleTime;    ///< Unix time of the pending switch-off; 0 = none.
} PayloadResponseGetStatus_t;
#pragma pack()

/**
 * @name CMD_GET_STATUS for every motor of a device
 * A request with moduleIndex = LORA_MODULE_ALL is answered with one
 * PayloadResponseGetStatusAll_t: a 6-byte header, then count records. The
 * device sends only the motors it has (1 .. LORA_STATUS_RECORDS_MAX). Its
 * autonomous reports use the same layout.
 * @{
 */
#define LORA_MODULE_ALL         0xFF ///< moduleIndex meaning "every motor of the device".
#define LORA_STATUS_RECORDS_MAX 6    ///< Largest count in PayloadResponseGetStatusAll_t.

/** @brief Status of one motor inside PayloadResponseGetStatusAll_t (13 bytes). */
#pragma pack(1)
typedef struct PayloadStatusRecord_t
{
    uint8_t moduleIndex;      ///< Motor module, 0 .. LORA_STATUS_RECORDS_MAX-1.
    uint8_t result;           ///< 1 = valid status.
    uint8_t powerState;       ///< A Motor_PowerState_e value.
    uint16_t phaseCurrent[3]; ///< Phase currents [0.01 A].
    uint32_t scheduleTime;    ///< Unix time of the pending switch-off; 0 = none.
} PayloadStatusRecord_t;
#pragma pack()

/** @brief Response payload of CMD_GET_STATUS with moduleIndex LORA_MODULE_ALL; only count records are sent. */
#pragma pack(1)
typedef struct PayloadResponseGetStatusAll_t
{
    uint8_t moduleIndex; ///< LORA_MODULE_ALL.
    uint32_t localTime;  ///< Device Unix time.
    uint8_t count;       ///< Records that follow, 1 .. LORA_STATUS_RECORDS_MAX.
    PayloadStatusRecord_t records[LORA_STATUS_RECORDS_MAX]; ///< One per motor.
} PayloadResponseGetStatusAll_t;
#pragma pack()
/** @} */

/** @brief Response payload of CMD_GET_FACTORY_INFO. */
#pragma pack(1)
typedef struct PayloadResponseGetFactoryInfo_t
{
    uint8_t result;         ///< 1 = valid.
    uint32_t serial;        ///< Device serial number.
    uint8_t deviceType;     ///< A Device_Type_e value.
    uint8_t numberOfModule; ///< Motor modules on the device.
} PayloadResponseGetFactoryInfo_t;
#pragma pack()

/** @brief Response payload of CMD_CONFIG_CONNECTION. */
#pragma pack(1)
typedef struct PayloadResponseConfigConnection_t
{
    uint8_t result;  ///< 1 = the device took the new settings.
    uint32_t serial; ///< Serial number of the device that answered.
} PayloadResponseConfigConnection_t;
#pragma pack()

/* Package builders */

LoraPackage_t Comm_CreateNewPackage(uint8_t netId, uint8_t nodeId, uint8_t opcode, uint8_t type, uint8_t *payload, int16_t payloadLength, uint32_t timeout);
void Comm_UpdatePackagePayload(LoraPackage_t *packageOut, uint8_t *payload, int16_t payloadLength);
LoraPackage_t Comm_CreateReqGetStatus(uint8_t netId, uint8_t nodeId, PayloadRequestGetStatus_t payload);
LoraPackage_t Comm_CreateReqControlPower(uint8_t netId, uint8_t nodeId, uint8_t moduleIndex, uint8_t powerState);
LoraPackage_t Comm_CreateReqSetSchedule(uint8_t netId, uint8_t nodeId, PayloadRequestSetSchedule_t payload);
LoraPackage_t Comm_CreateReqSyncTime(uint8_t netId, uint8_t nodeId, PayloadRequestSyncTime_t payload);
LoraPackage_t Comm_CreateReqConfigConnection(uint8_t netId, uint8_t nodeId, PayloadRequestConfigConnection_t payload);
LoraPackage_t Comm_CreateReqGetFactoryInfo(uint8_t netId, uint8_t nodeId);

/* Receive path */

void Comm_ProcessRxData(uint8_t *buffer, uint32_t length);

/* Payload parsers */

uint8_t Comm_ParsePayloadReqCtrlPwr(LoraPackage_t package, PayloadRequestCtrlPower_t *payload);
uint8_t Comm_ParsePayloadReqSetSchedule(LoraPackage_t package, PayloadRequestSetSchedule_t *payload);
uint8_t Comm_ParsePayloadReqSyncTime(LoraPackage_t package, PayloadRequestSyncTime_t *payload);
uint8_t Comm_ParsePayloadReqConfigConnection(LoraPackage_t package, PayloadRequestConfigConnection_t *payload);
uint8_t Comm_ParsePayloadReqAlarm(LoraPackage_t package, PayloadRequestAlarm_t *payload);
uint8_t Comm_ParsePayloadResGetStatus(LoraPackage_t package, PayloadResponseGetStatus_t *payload);
uint8_t Comm_ParsePayloadResGetStatusAll(LoraPackage_t package, PayloadResponseGetStatusAll_t *payload);
uint8_t Comm_ParsePayloadResGetFactoryInfo(LoraPackage_t package, PayloadResponseGetFactoryInfo_t *payload);
uint8_t Comm_ParsePayloadResConfigConnection(LoraPackage_t package, PayloadResponseConfigConnection_t *payload);

#endif
/** @} */
