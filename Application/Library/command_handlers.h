/**
 * @file    command_handlers.h
 * @brief   Handlers for received LoRa commands, and the SET_CONFIG request.
 *
 * Comm_ProcessRxData() looks up the command code in cmdTable and calls the
 * matching static handler in command_handlers.c, which validates the frame
 * and passes it to the module that owns the data.
 *
 * @defgroup cmdhandler Command handlers
 * @{
 */
#ifndef COMMAND_HANDLERS_H
#define COMMAND_HANDLERS_H
#include "stdint.h"
#include "communication_lora.h"

/** @brief Handler for one received command. */
typedef void (*CmdHandler_t)(LoraPackage_t fwPackage);

/** @brief One cmdTable entry. */
typedef struct
{
    uint8_t cmdCode;      ///< Command code (CMD_...).
    CmdHandler_t handler; ///< Handler called for that code.
} CmdMapping_t;

/** @brief Command dispatch table, terminated by a NULL handler. */
extern const CmdMapping_t cmdTable[10];

uint8_t CmdHandler_SendCurrentModuleConfig(void);

#endif
/** @} */
