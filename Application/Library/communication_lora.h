#ifndef COMMUNICATION_LORA_H
#define COMMUNICATION_LORA_H
#include "stdint.h"

#define LORA_PACKAGE_SIGNATURE          0xFF55
#define LORA_PACKAGE_HEADER_SIZE        0x09
#define LORA_PACKAGE_RAW_DATA_SIZE      256
#define LORA_PACKAGE_CRC_SIZE           0x01
#define LORA_PACKAGE_SERIAL_NUMBER_SIZE 0x0C
#define LORA_PACKAGE_TYPE_REQUEST       0x01
#define LORA_PACKAGE_TYPE_RESPONSE      0x02

#define CMD_GET_STATUS        0x01
#define CMD_CONTROL_POWER     0x02
#define CMD_SET_SCHEDULE      0x03
#define CMD_SYNC_TIME         0x04
#define CMD_GET_FACTORY_INFO  0x05
#define CMD_CONFIG_CONNECTION 0x06
#define CMD_ALARM_MESSAGE     0x07
#define CMD_SET_CONFIG        0x08

#define LORA_NODE_ID_BROADCAST 0xFF
#define LORA_NET_ID_DEFAULT    0x00
#define LORA_NODE_ID_DEFAULT   0x00

/* Wire units: 0.01 A and seconds. Single-motor slave supports module 0. */
#pragma pack(1)
typedef struct
{
    uint8_t moduleIndex;
    uint16_t alarmThresholdCentiAmp;
    uint16_t alarmDelaySeconds;
    uint16_t autoStatusSeconds;
} PayloadRequestSetConfig_t;
#pragma pack()

#pragma pack(1)
typedef struct LoraPackage_t
{
    uint16_t signature;
    uint8_t length;
    uint8_t netId;
    uint8_t nodeId;
    uint8_t type;
    uint16_t transactionID;
    uint8_t code;
    uint8_t checkSum;
    uint32_t byteCount;
    uint32_t timeout; /* ms */
    uint8_t rawData[LORA_PACKAGE_RAW_DATA_SIZE];
} LoraPackage_t;
#pragma pack()

/* REQUEST PAYLOAD ========================================================== */

/* Payload for CMD_GET_STATUS */
#pragma pack(1)
typedef struct PayloadRequestGetStatus_t
{
    uint8_t moduleIndex;
    uint32_t syncTime;
} PayloadRequestGetStatus_t;
#pragma pack()

/* Payload for CMD_CONTROL_POWER */
#pragma pack(1)
typedef struct PayloadRequestCtrlPower_t
{
    uint8_t moduleIndex;
    uint8_t powerState;
} PayloadRequestCtrlPower_t;
#pragma pack()

/* Payload for CMD_SET_SCHEDULE */
#pragma pack(1)
typedef struct PayloadRequestSetSchedule_t
{
    uint8_t moduleIndex;
    uint32_t syncTime;
    uint32_t scheduleTime;
} PayloadRequestSetSchedule_t;
#pragma pack()

/* Payload for CMD_SYNC_TIME */
#pragma pack(1)
typedef struct PayloadRequestSyncTime_t
{
    uint32_t unixTime;
} PayloadRequestSyncTime_t;
#pragma pack()

/* Payload for CMD_GET_FACTORY_INFO: none. */

/* Payload for CMD_CONFIG_CONNECTION */
#pragma pack(1)
typedef struct PayloadRequestConfigConnection_t
{
    uint32_t serial;
    uint8_t netId;
    uint8_t nodeId;
    uint32_t frequency;
    uint8_t sf;
} PayloadRequestConfigConnection_t;
#pragma pack()

/* Payload for CMD_ALARM_MESSAGE */
#pragma pack(1)
typedef struct PayloadRequestAlarm_t
{
    uint8_t moduleIndex;
    uint8_t alarmCode;
    uint32_t alarmTime;
} PayloadRequestAlarm_t;
#pragma pack()

/* RESPONSE PAYLOAD ========================================================= */

#pragma pack(1)
typedef struct PayloadCommonResponse_t
{
    uint8_t moduleIndex;
    uint8_t result;
} PayloadCommonResponse_t;
#pragma pack()

/* Payload for CMD_GET_STATUS */
#pragma pack(1)
typedef struct PayloadResponseGetStatus_t
{
    uint8_t moduleIndex;
    uint8_t result;
    uint8_t powerState;
    uint16_t phaseCurrent[3];
    uint32_t localTime;
    uint32_t scheduleTime;
} PayloadResponseGetStatus_t;
#pragma pack()

/* Payload for CMD_CONTROL_POWER, CMD_SET_SCHEDULE, CMD_SYNC_TIME: PayloadCommonResponse_t. */

/* Payload for CMD_GET_FACTORY_INFO */
#pragma pack(1)
typedef struct PayloadResponseGetFactoryInfo_t
{
    uint8_t result;
    uint32_t serial;
    uint8_t deviceType;
    uint8_t numberOfModule;
} PayloadResponseGetFactoryInfo_t;
#pragma pack()

/* Payload for CMD_CONFIG_CONNECTION */
#pragma pack(1)
typedef struct PayloadResponseConfigConnection_t
{
    uint8_t result;
    uint32_t serial;
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
uint8_t Comm_ParsePayloadResGetFactoryInfo(LoraPackage_t package, PayloadResponseGetFactoryInfo_t *payload);
uint8_t Comm_ParsePayloadResConfigConnection(LoraPackage_t package, PayloadResponseConfigConnection_t *payload);

#endif
