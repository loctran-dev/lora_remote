#ifndef REMOTE_HISTORY_H
#define REMOTE_HISTORY_H
#include "communication_lora.h"
#include "history_journal.h"

/* Diagnostics counters, reported over USB. */
extern uint32_t remoteHistoryStored;
extern uint32_t remoteHistoryDuplicates;
extern uint32_t remoteHistoryAcked;
extern uint32_t remoteHistoryRejected;
extern uint32_t remoteHistoryRevision;

void RemoteHistory_Init(void);
uint16_t RemoteHistory_Owner(uint8_t id);

/* Event sources */
void RemoteHistory_Handle(LoraPackage_t packet);
void RemoteHistory_Link(uint8_t id, uint8_t restored);

/* Storage and acknowledgement */
uint8_t RemoteHistory_Busy(void);
void RemoteHistory_Tick(void);

#endif
