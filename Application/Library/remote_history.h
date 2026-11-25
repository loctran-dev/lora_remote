/**
 * @file    remote_history.h
 * @brief   Collects history events from the devices and the link monitor into the journal.
 *
 * Events arrive from the receive path (CMD_HISTORY pushes) and from the link
 * monitor (link lost/restored). They are queued in RAM and written to flash
 * by RemoteHistory_Tick() in systemTask; device events are then acknowledged
 * with the original transaction id so the device can stop resending them.
 *
 * @ingroup history
 * @{
 */
#ifndef REMOTE_HISTORY_H
#define REMOTE_HISTORY_H
#include "communication_lora.h"
#include "history_journal.h"

/** @name Diagnostic counters, reported over USB */
/** @{ */
extern uint32_t remoteHistoryStored;     ///< New events written to the journal.
extern uint32_t remoteHistoryDuplicates; ///< Events that were already stored.
extern uint32_t remoteHistoryAcked;      ///< Acknowledgements queued.
extern uint32_t remoteHistoryRejected;   ///< Invalid, conflicting or unstorable events.
extern uint32_t remoteHistoryRevision;   ///< Incremented on every new event; the history screen compares it.
/** @} */

void RemoteHistory_Init(void);
uint16_t RemoteHistory_Owner(uint8_t id);

/* Event sources */

void RemoteHistory_Handle(LoraPackage_t packet);
void RemoteHistory_Link(uint8_t id, uint8_t restored);

/* Storage and acknowledgement */

uint8_t RemoteHistory_Busy(void);
void RemoteHistory_Tick(void);

#endif
/** @} */
