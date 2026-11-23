#ifndef HISTORY_JOURNAL_H
#define HISTORY_JOURNAL_H
#include <stdint.h>
#include "history_port.h"

#define CMD_HISTORY 0x09

/* HistoryEvent.flags */
#define HIST_TIME_UNKNOWN 1u
#define HIST_LOCAL        128u

/* HistoryEvent.error */
#define HIST_ERROR_LOW_CURRENT 1u
#define HIST_ERROR_NO_STATUS   256u

/* Journal geometry; HJ_REMOTE, HJ_BASE, HJ_SIZE and HJ_PAGE_SIZE come from history_port.h. */
#define HJ_HEADER_SIZE (HJ_REMOTE ? 256u : 32u)
#define HJ_PAGES       (HJ_SIZE / HJ_PAGE_SIZE)
#define HJ_PER_PAGE    ((HJ_PAGE_SIZE - HJ_HEADER_SIZE) / 32u)
#define HJ_SLOTS       (HJ_PAGES * HJ_PER_PAGE)

/* HistoryEvent.action */
enum
{
    HIST_ON = 1,
    HIST_OFF,
    HIST_ALARM,
    HIST_CLEAR,
    HIST_LINK_LOST,
    HIST_LINK_RESTORED
};

/* HistoryEvent.source */
enum
{
    HIST_REMOTE = 1,
    HIST_IO1,
    HIST_IO2,
    HIST_IO3,
    HIST_IO4,
    HIST_TIMER,
    HIST_SLAVE,
    HIST_MONITOR
};

/* HistoryEvent.result */
enum
{
    HIST_APPLIED = 1,
    HIST_UNCHANGED,
    HIST_REJECTED,
    HIST_WARNING
};

/* Field order is part of the radio and flash format: do not reorder. */
typedef struct
{
    uint32_t serial;
    uint32_t id;
    uint32_t timestamp;
    uint16_t error;
    uint8_t action;
    uint8_t source;
    uint8_t before;
    uint8_t after;
    uint8_t result;
    uint8_t flags;
} HistoryEvent;

typedef struct
{
    HistoryEvent event;
    uint32_t crc;
} HistoryPush;

typedef struct
{
    uint32_t serial;
    uint32_t id;
    uint32_t eventCrc;
} HistoryAck;

typedef struct
{
    HistoryEvent event;
    uint16_t owner;
    uint16_t synced;
    uint32_t crc;
    uint32_t commit;
} HistoryRecord;

typedef struct
{
    uint32_t serial;
    uint32_t id;
    uint16_t owner;
    uint16_t reserved;
} HistoryCursor;

extern uint32_t historyStoreErrors;
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
