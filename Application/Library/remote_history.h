#ifndef REMOTE_HISTORY_H
#define REMOTE_HISTORY_H
#include "communicationLora.h"
#include "history_journal.h"
void RemoteHistory_Init(void);
void RemoteHistory_Handle(LoraPackage_t packet);
void RemoteHistory_Tick(void);
uint8_t RemoteHistory_Busy(void);
void RemoteHistory_Link(uint8_t id, uint8_t restored);
uint16_t RemoteHistory_Owner(uint8_t id);
extern uint32_t remoteHistoryStored, remoteHistoryDuplicates, remoteHistoryAcked, remoteHistoryRejected, remoteHistoryRevision;
void Screen_OpenHistory(void);
extern uint8_t historyScreenMode;
#endif
