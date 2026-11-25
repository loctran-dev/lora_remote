/**
 * @file    history_journal.h
 * @brief   Power-loss-safe event journal in flash, shared with the device firmware.
 *
 * The journal is a ring of flash pages, each with a header and 32-byte
 * records. A record or page header becomes valid only when its commit word,
 * written last, is present, so an interrupted write is ignored at the next
 * HJ_Init(). On the remote (HJ_REMOTE) every page header also keeps per-owner
 * cursors, so events that have already been stored and later overwritten are
 * not stored again when a device resends them.
 *
 * @defgroup history History
 * @{
 */
#ifndef HISTORY_JOURNAL_H
#define HISTORY_JOURNAL_H
#include <stdint.h>
#include "history_port.h"

/** @brief Command code of history push (request) and acknowledgement (response) frames. */
#define CMD_HISTORY 0x09

/** @name HistoryEvent.flags */
/** @{ */
#define HIST_TIME_UNKNOWN 1u   ///< The timestamp came from an unset clock.
#define HIST_LOCAL        128u ///< Created by the remote itself (link events), not by a device.
/** @} */

/** @name HistoryEvent.error */
/** @{ */
#define HIST_ERROR_LOW_CURRENT 1u   ///< Motor current below the alarm threshold.
#define HIST_ERROR_NO_STATUS   256u ///< No status reply from the device.
/** @} */

/** @name Journal geometry */
/** @{ */
#define HJ_HEADER_SIZE (HJ_REMOTE ? 256u : 32u)            ///< Page header size [bytes].
#define HJ_PAGES       (HJ_SIZE / HJ_PAGE_SIZE)            ///< Pages in the journal.
#define HJ_PER_PAGE    ((HJ_PAGE_SIZE - HJ_HEADER_SIZE) / 32u) ///< Records per page.
#define HJ_SLOTS       (HJ_PAGES * HJ_PER_PAGE)            ///< Records in the journal.
/** @} */

/** @brief HistoryEvent.action values. */
enum
{
    HIST_ON = 1,       ///< Motor switched on.
    HIST_OFF,          ///< Motor switched off.
    HIST_ALARM,        ///< Alarm raised.
    HIST_CLEAR,        ///< Alarm cleared.
    HIST_LINK_LOST,    ///< The remote lost the link to the device.
    HIST_LINK_RESTORED ///< The link came back.
};

/** @brief HistoryEvent.source values. */
enum
{
    HIST_REMOTE = 1, ///< Command from the remote.
    HIST_IO1,        ///< Device input 1.
    HIST_IO2,        ///< Device input 2.
    HIST_IO3,        ///< Device input 3.
    HIST_IO4,        ///< Device input 4.
    HIST_TIMER,      ///< Schedule expired.
    HIST_SLAVE,      ///< Decided by the device itself.
    HIST_MONITOR     ///< Link monitor on the remote.
};

/** @brief HistoryEvent.result values. */
enum
{
    HIST_APPLIED = 1, ///< The state changed as requested.
    HIST_UNCHANGED,   ///< Already in the requested state.
    HIST_REJECTED,    ///< Refused (for example a locked motor).
    HIST_WARNING      ///< Applied with a warning, or a warning-only event.
};

/** @brief One history event. Field order is part of the radio and flash format: do not reorder. */
typedef struct
{
    uint32_t serial;    ///< Serial number of the device.
    uint32_t id;        ///< Event id, increasing per device; assigned by HJ_Append() when 0.
    uint32_t timestamp; ///< Unix time of the event.
    uint16_t error;     ///< HIST_ERROR_... bits.
    uint8_t action;     ///< HIST_ON ... HIST_LINK_RESTORED.
    uint8_t source;     ///< HIST_REMOTE ... HIST_MONITOR.
    uint8_t before;     ///< Motor state before, a Motor_PowerState_e value.
    uint8_t after;      ///< Motor state after, a Motor_PowerState_e value.
    uint8_t result;     ///< HIST_APPLIED ... HIST_WARNING.
    uint8_t flags;      ///< HIST_TIME_UNKNOWN, HIST_LOCAL.
} HistoryEvent;

/** @brief Payload of a CMD_HISTORY request sent by a device. */
typedef struct
{
    HistoryEvent event; ///< The event.
    uint32_t crc;       ///< HJ_Crc() of event.
} HistoryPush;

/** @brief Payload of the CMD_HISTORY response that acknowledges a stored event. */
typedef struct
{
    uint32_t serial;   ///< Event serial.
    uint32_t id;       ///< Event id.
    uint32_t eventCrc; ///< HJ_Crc() of the stored event.
} HistoryAck;

/** @brief One 32-byte journal record as stored in flash. */
typedef struct
{
    HistoryEvent event; ///< The event.
    uint16_t owner;     ///< (netId << 8) | nodeId of the device, see RemoteHistory_Owner().
    uint16_t synced;    ///< 0xFFFF until HJ_MarkSynced() programs it to 0.
    uint32_t crc;       ///< HJ_Crc() of event and owner.
    uint32_t commit;    ///< Written last; the record is valid only when it is set.
} HistoryRecord;

/** @brief Highest event id already stored for one device (remote only). */
typedef struct
{
    uint32_t serial;   ///< Device serial.
    uint32_t id;       ///< Highest stored event id.
    uint16_t owner;    ///< Owner of the device.
    uint16_t reserved; ///< Padding.
} HistoryCursor;

/** @brief Failed flash programs, for diagnostics. */
extern uint32_t historyStoreErrors;
/** @brief Events not stored because the event id space is exhausted. */
extern uint32_t historyStoreFull;

uint32_t HJ_Crc(const void *data, uint32_t size);
void HJ_Init(void);

/* Record access */

const HistoryRecord *HJ_Get(uint16_t slot);
uint8_t HJ_Contains(const HistoryEvent *event, uint16_t owner);
uint8_t HJ_Append(HistoryEvent *event, uint16_t owner);

/* Synchronisation */

const HistoryRecord *HJ_Pending(void);
uint8_t HJ_MarkSynced(uint32_t serial, uint32_t id);

#endif
/** @} */
