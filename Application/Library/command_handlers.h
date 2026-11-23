#ifndef COMMAND_HANDLERS_H
#define COMMAND_HANDLERS_H
#include "stdint.h"
#include "communication_lora.h"

typedef void (*CmdHandler_t)(LoraPackage_t fwPackage);

typedef struct
{
    uint8_t cmdCode;
    CmdHandler_t handler;
} CmdMapping_t;

extern const CmdMapping_t cmdTable[10];

uint8_t CmdHandler_SendCurrentModuleConfig(void);

#endif
