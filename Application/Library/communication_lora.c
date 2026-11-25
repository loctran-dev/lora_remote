#include "communication_lora.h"
#include "string.h"
#include "utility.h"
#include "lora.h"
#include "main.h"
#include "command_handlers.h"

#define LORA_PACKAGE_SIGNATURE 0xFF55

static uint16_t currentTransactionID = 0;

static uint16_t generateTransactionID(void);
static void executeCommand(LoraPackage_t package);

/* Package builders ========================================================= */

/**
 * @brief  Replace the payload of a built frame and recompute length and checksum.
 * @param  packageOut     Frame to update in place; header fields are kept.
 * @param  payload        New payload bytes.
 * @param  payloadLength  Payload size [bytes].
 */
void Comm_UpdatePackagePayload(LoraPackage_t *packageOut, uint8_t *payload, int16_t payloadLength)
{
    LoraPackage_t packageIn = *packageOut;
    packageIn.length = LORA_PACKAGE_HEADER_SIZE - sizeof(packageOut->signature) + payloadLength;
    packageOut->length = packageIn.length;
    memcpy(packageOut->rawData, &packageIn, LORA_PACKAGE_HEADER_SIZE);
    memcpy(&packageOut->rawData[LORA_PACKAGE_HEADER_SIZE], payload, payloadLength);
    packageOut->byteCount = LORA_PACKAGE_HEADER_SIZE + payloadLength + sizeof(packageOut->checkSum);
    packageOut->checkSum = Util_CalculateCRC(&packageOut->rawData[0], packageOut->length);
    memcpy(&packageOut->rawData[packageOut->byteCount - 1], &packageOut->checkSum, sizeof(packageOut->checkSum));
}

/**
 * @brief  Build a complete frame with a new transaction id.
 * @param  netId          Network id.
 * @param  nodeId         Destination node id.
 * @param  opcode         Command code (CMD_...).
 * @param  type           LORA_PACKAGE_TYPE_REQUEST or LORA_PACKAGE_TYPE_RESPONSE.
 * @param  payload        Payload bytes; not read when payloadLength is 0.
 * @param  payloadLength  Payload size [bytes].
 * @param  timeout        Stored in LoraPackage_t.timeout [ms].
 * @return The frame, ready to queue on txMsgQueue.
 */
LoraPackage_t Comm_CreateNewPackage(uint8_t netId, uint8_t nodeId, uint8_t opcode, uint8_t type, uint8_t *payload, int16_t payloadLength, uint32_t timeout)
{
    LoraPackage_t packageOut;
    memset(packageOut.rawData, 0x00, LORA_PACKAGE_RAW_DATA_SIZE);
    packageOut.signature = LORA_PACKAGE_SIGNATURE;
    packageOut.length = LORA_PACKAGE_HEADER_SIZE - sizeof(packageOut.signature) + payloadLength;
    packageOut.netId = netId;
    packageOut.nodeId = nodeId;
    packageOut.type = type;
    packageOut.transactionID = generateTransactionID();
    packageOut.code = opcode;
    packageOut.byteCount = LORA_PACKAGE_HEADER_SIZE + payloadLength + sizeof(packageOut.checkSum);
    packageOut.timeout = timeout; /* ms */

    memcpy(packageOut.rawData, &packageOut, LORA_PACKAGE_HEADER_SIZE);
    memcpy(&packageOut.rawData[LORA_PACKAGE_HEADER_SIZE], payload, payloadLength);

    packageOut.checkSum = Util_CalculateCRC(&packageOut.rawData[0], packageOut.length);
    memcpy(&packageOut.rawData[packageOut.byteCount - 1], &packageOut.checkSum, sizeof(packageOut.checkSum));

    return packageOut;
}

/**
 * @brief  Build a CMD_GET_STATUS request.
 * @param  netId    Network id.
 * @param  nodeId   Destination node id.
 * @param  payload  Module and clock to sync.
 * @return The frame.
 */
LoraPackage_t Comm_CreateReqGetStatus(uint8_t netId, uint8_t nodeId, PayloadRequestGetStatus_t payload)
{
    return Comm_CreateNewPackage(netId, nodeId, CMD_GET_STATUS, LORA_PACKAGE_TYPE_REQUEST, (uint8_t *)&payload, sizeof(payload), 0);
}

/**
 * @brief  Build a CMD_CONTROL_POWER request.
 * @param  netId        Network id.
 * @param  nodeId       Destination node id.
 * @param  moduleIndex  Motor module, 0.
 * @param  powerState   A Motor_PowerState_e value.
 * @return The frame.
 */
LoraPackage_t Comm_CreateReqControlPower(uint8_t netId, uint8_t nodeId, uint8_t moduleIndex, uint8_t powerState)
{
    uint8_t payload[2];
    payload[0] = moduleIndex;
    payload[1] = powerState;
    return Comm_CreateNewPackage(netId, nodeId, CMD_CONTROL_POWER, LORA_PACKAGE_TYPE_REQUEST, payload, sizeof(payload), 0);
}

/**
 * @brief  Build a CMD_SET_SCHEDULE request.
 * @param  netId    Network id.
 * @param  nodeId   Destination node id.
 * @param  payload  Switch-off time, or 0 to cancel.
 * @return The frame.
 */
LoraPackage_t Comm_CreateReqSetSchedule(uint8_t netId, uint8_t nodeId, PayloadRequestSetSchedule_t payload)
{
    return Comm_CreateNewPackage(netId, nodeId, CMD_SET_SCHEDULE, LORA_PACKAGE_TYPE_REQUEST, (uint8_t *)&payload, sizeof(payload), 0);
}

/**
 * @brief  Build a CMD_SYNC_TIME request.
 * @param  netId    Network id.
 * @param  nodeId   Destination node id.
 * @param  payload  New device time.
 * @return The frame.
 */
LoraPackage_t Comm_CreateReqSyncTime(uint8_t netId, uint8_t nodeId, PayloadRequestSyncTime_t payload)
{
    return Comm_CreateNewPackage(netId, nodeId, CMD_SYNC_TIME, LORA_PACKAGE_TYPE_REQUEST, (uint8_t *)&payload, sizeof(payload), 0);
}

/**
 * @brief  Build a CMD_GET_FACTORY_INFO request (no payload).
 * @param  netId   Network id; LORA_NET_ID_DEFAULT while pairing.
 * @param  nodeId  Destination node id; LORA_NODE_ID_DEFAULT while pairing.
 * @return The frame.
 */
LoraPackage_t Comm_CreateReqGetFactoryInfo(uint8_t netId, uint8_t nodeId)
{
    uint8_t payload[1];
    return Comm_CreateNewPackage(netId, nodeId, CMD_GET_FACTORY_INFO, LORA_PACKAGE_TYPE_REQUEST, payload, 0, 0);
}

/**
 * @brief  Build a CMD_CONFIG_CONNECTION request.
 * @param  netId    Network id the device listens on (LORA_NET_ID_DEFAULT while pairing).
 * @param  nodeId   Node id the device listens on (LORA_NODE_ID_DEFAULT while pairing).
 * @param  payload  New network settings for the device.
 * @return The frame.
 */
LoraPackage_t Comm_CreateReqConfigConnection(uint8_t netId, uint8_t nodeId, PayloadRequestConfigConnection_t payload)
{
    return Comm_CreateNewPackage(netId, nodeId, CMD_CONFIG_CONNECTION, LORA_PACKAGE_TYPE_REQUEST, (uint8_t *)&payload, sizeof(payload), 0);
}

/* Receive path ============================================================= */

/**
 * @brief  Find, validate and dispatch one received frame.
 *
 * Looks for the signature, checks length and checksum and passes the frame
 * to the matching cmdTable handler. Invalid data is dropped silently.
 *
 * @param  buffer  Bytes read from the radio.
 * @param  length  Number of bytes in buffer.
 * @note   Called by loraReceiveTask with radioMutex held.
 */
void Comm_ProcessRxData(uint8_t *buffer, uint32_t length)
{
    uint8_t *frameStart = NULL;
    uint32_t frameLength = 0;
    LoraPackage_t fwPackage;
    uint8_t readCRC;

    if (length < 3)
    {
        return;
    }

    /* Find the frame signature. */
    for (uint32_t i = 0; i < (length - 1); i++)
    {
        if (*(uint16_t *)&buffer[i] == LORA_PACKAGE_SIGNATURE)
        {
            frameStart = &buffer[i];
            frameLength = length - i;
            break;
        }
    }

    if (frameStart == NULL || frameLength < LORA_PACKAGE_HEADER_SIZE + LORA_PACKAGE_CRC_SIZE)
    {
        return;
    }

    /* Decode the header and validate the frame length. */
    memset(fwPackage.rawData, 0x00, LORA_PACKAGE_RAW_DATA_SIZE);
    memcpy(&fwPackage, frameStart, LORA_PACKAGE_HEADER_SIZE);
    fwPackage.byteCount = fwPackage.length + sizeof(fwPackage.signature) + sizeof(fwPackage.checkSum);
    if (fwPackage.byteCount > frameLength || fwPackage.byteCount > LORA_PACKAGE_RAW_DATA_SIZE ||
        fwPackage.byteCount < LORA_PACKAGE_HEADER_SIZE + LORA_PACKAGE_CRC_SIZE)
    {
        return;
    }

    /* Copy the full frame and verify its checksum. */
    memcpy(fwPackage.rawData, frameStart, fwPackage.byteCount);
    fwPackage.checkSum = frameStart[fwPackage.byteCount - 1];
    readCRC = Util_CalculateCRC(&fwPackage.rawData[0], fwPackage.length);
    if (readCRC != fwPackage.checkSum)
    {
        return;
    }

    executeCommand(fwPackage);
}

/* Payload parsers ========================================================== */

/**
 * @brief  Copy the payload of a CMD_CONTROL_POWER request.
 * @param  package  Received frame.
 * @param  payload  Filled on success.
 * @return TRUE if the code and payload size match, FALSE otherwise.
 */
uint8_t Comm_ParsePayloadReqCtrlPwr(LoraPackage_t package, PayloadRequestCtrlPower_t *payload)
{
    int payloadLength = package.byteCount - LORA_PACKAGE_HEADER_SIZE - sizeof(package.checkSum);
    if (package.code != CMD_CONTROL_POWER)
    {
        return FALSE;
    }
    if (payloadLength != sizeof(PayloadRequestCtrlPower_t))
    {
        return FALSE;
    }
    memset(payload, 0x00, sizeof(PayloadRequestCtrlPower_t));
    memcpy(payload, &package.rawData[LORA_PACKAGE_HEADER_SIZE], payloadLength);
    return TRUE;
}

/**
 * @brief  Copy the payload of a CMD_SET_SCHEDULE request.
 * @param  package  Received frame.
 * @param  payload  Filled on success.
 * @return TRUE if the code and payload size match, FALSE otherwise.
 */
uint8_t Comm_ParsePayloadReqSetSchedule(LoraPackage_t package, PayloadRequestSetSchedule_t *payload)
{
    int payloadLength = package.byteCount - LORA_PACKAGE_HEADER_SIZE - sizeof(package.checkSum);
    if (package.code != CMD_SET_SCHEDULE)
    {
        return FALSE;
    }
    if (payloadLength != sizeof(PayloadRequestSetSchedule_t))
    {
        return FALSE;
    }
    memset(payload, 0x00, sizeof(PayloadRequestSetSchedule_t));
    memcpy(payload, &package.rawData[LORA_PACKAGE_HEADER_SIZE], payloadLength);
    return TRUE;
}

/**
 * @brief  Copy the payload of a CMD_SYNC_TIME request.
 * @param  package  Received frame.
 * @param  payload  Filled on success.
 * @return TRUE if the code and payload size match, FALSE otherwise.
 */
uint8_t Comm_ParsePayloadReqSyncTime(LoraPackage_t package, PayloadRequestSyncTime_t *payload)
{
    int payloadLength = package.byteCount - LORA_PACKAGE_HEADER_SIZE - sizeof(package.checkSum);
    if (package.code != CMD_SYNC_TIME)
    {
        return FALSE;
    }
    if (payloadLength != sizeof(PayloadRequestSyncTime_t))
    {
        return FALSE;
    }
    memset(payload, 0x00, sizeof(PayloadRequestSyncTime_t));
    memcpy(payload, &package.rawData[LORA_PACKAGE_HEADER_SIZE], payloadLength);
    return TRUE;
}

/**
 * @brief  Copy the payload of a CMD_CONFIG_CONNECTION request.
 * @param  package  Received frame.
 * @param  payload  Filled on success.
 * @return TRUE if the code and payload size match, FALSE otherwise.
 */
uint8_t Comm_ParsePayloadReqConfigConnection(LoraPackage_t package, PayloadRequestConfigConnection_t *payload)
{
    int payloadLength = package.byteCount - LORA_PACKAGE_HEADER_SIZE - sizeof(package.checkSum);
    if (package.code != CMD_CONFIG_CONNECTION)
    {
        return FALSE;
    }
    if (payloadLength != sizeof(PayloadRequestConfigConnection_t))
    {
        return FALSE;
    }
    memset(payload, 0x00, sizeof(PayloadRequestConfigConnection_t));
    memcpy(payload, &package.rawData[LORA_PACKAGE_HEADER_SIZE], payloadLength);
    return TRUE;
}

/**
 * @brief  Copy the payload of a CMD_ALARM_MESSAGE sent by a device.
 * @param  package  Received frame.
 * @param  payload  Filled on success.
 * @return TRUE if the code and payload size match, FALSE otherwise.
 */
uint8_t Comm_ParsePayloadReqAlarm(LoraPackage_t package, PayloadRequestAlarm_t *payload)
{
    int payloadLength = package.byteCount - LORA_PACKAGE_HEADER_SIZE - sizeof(package.checkSum);
    if (package.code != CMD_ALARM_MESSAGE)
    {
        return FALSE;
    }
    if (payloadLength != sizeof(PayloadRequestAlarm_t))
    {
        return FALSE;
    }
    memset(payload, 0x00, sizeof(PayloadRequestAlarm_t));
    memcpy(payload, &package.rawData[LORA_PACKAGE_HEADER_SIZE], payloadLength);
    return TRUE;
}

/**
 * @brief  Copy the payload of a CMD_GET_STATUS response.
 * @param  package  Received frame.
 * @param  payload  Filled on success.
 * @return TRUE if the code and payload size match, FALSE otherwise.
 */
uint8_t Comm_ParsePayloadResGetStatus(LoraPackage_t package, PayloadResponseGetStatus_t *payload)
{
    int payloadLength = package.byteCount - LORA_PACKAGE_HEADER_SIZE - sizeof(package.checkSum);
    if (package.code != CMD_GET_STATUS)
    {
        return FALSE;
    }
    if (payloadLength != sizeof(PayloadResponseGetStatus_t))
    {
        return FALSE;
    }
    memset(payload, 0x00, sizeof(PayloadResponseGetStatus_t));
    memcpy(payload, &package.rawData[LORA_PACKAGE_HEADER_SIZE], payloadLength);
    return TRUE;
}

/**
 * @brief  Copy the payload of a CMD_GET_FACTORY_INFO response.
 * @param  package  Received frame.
 * @param  payload  Filled on success.
 * @return TRUE if the code and payload size match, FALSE otherwise.
 */
uint8_t Comm_ParsePayloadResGetFactoryInfo(LoraPackage_t package, PayloadResponseGetFactoryInfo_t *payload)
{
    int payloadLength = package.byteCount - LORA_PACKAGE_HEADER_SIZE - sizeof(package.checkSum);
    if (package.code != CMD_GET_FACTORY_INFO)
    {
        return FALSE;
    }
    if (payloadLength != sizeof(PayloadResponseGetFactoryInfo_t))
    {
        return FALSE;
    }
    memset(payload, 0x00, sizeof(PayloadResponseGetFactoryInfo_t));
    memcpy(payload, &package.rawData[LORA_PACKAGE_HEADER_SIZE], payloadLength);
    return TRUE;
}

/**
 * @brief  Copy the payload of a CMD_CONFIG_CONNECTION response.
 * @param  package  Received frame.
 * @param  payload  Filled on success.
 * @return TRUE if the code and payload size match, FALSE otherwise.
 */
uint8_t Comm_ParsePayloadResConfigConnection(LoraPackage_t package, PayloadResponseConfigConnection_t *payload)
{
    int payloadLength = package.byteCount - LORA_PACKAGE_HEADER_SIZE - sizeof(package.checkSum);
    if (package.code != CMD_CONFIG_CONNECTION)
    {
        return FALSE;
    }
    if (payloadLength != sizeof(PayloadResponseConfigConnection_t))
    {
        return FALSE;
    }
    memset(payload, 0x00, sizeof(PayloadResponseConfigConnection_t));
    memcpy(payload, &package.rawData[LORA_PACKAGE_HEADER_SIZE], payloadLength);
    return TRUE;
}

/* Private helpers ========================================================== */

static uint16_t generateTransactionID(void)
{
    currentTransactionID++;
    if (!currentTransactionID)
    {
        currentTransactionID++; /* Zero is unsolicited STATUS. */
    }
    return currentTransactionID;
}

static void executeCommand(LoraPackage_t package)
{
    for (int i = 0; cmdTable[i].handler != NULL; i++)
    {
        if (cmdTable[i].cmdCode == package.code)
        {
            cmdTable[i].handler(package);
            return;
        }
    }
}
