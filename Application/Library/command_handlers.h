#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H
#include "stdint.h"
#include "communication_lora.h"

typedef void (*CmdHandler_t)(LoraPackage_t fwPackage);
uint8_t CmdHandler_SendCurrentModuleConfig(void);

typedef struct {
    uint8_t cmdCode;
    CmdHandler_t handler;
} CmdMapping_t;

extern const CmdMapping_t cmdTable[10];

#endif
