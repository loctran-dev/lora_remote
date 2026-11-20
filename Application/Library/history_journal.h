#ifndef HISTORY_JOURNAL_H
#define HISTORY_JOURNAL_H
#include <stdint.h>
#include "history_port.h"
#define CMD_HISTORY 0x09
enum
{
    HIST_ON = 1,
    HIST_OFF,
    HIST_ALARM,
    HIST_CLEAR,
    HIST_LINK_LOST,
    HIST_LINK_RESTORED
};
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
enum
{
    HIST_APPLIED = 1,
    HIST_UNCHANGED,
    HIST_REJECTED,
    HIST_WARNING
};
#define HIST_TIME_UNKNOWN 1u
#define HIST_LOCAL 128u
#define HIST_ERROR_LOW_CURRENT 1u
#define HIST_ERROR_NO_STATUS 256u
typedef struct
{
    uint32_t serial, id, timestamp;
    uint16_t error;
    uint8_t action, source, before, after, result, flags;
} HistoryEvent;
typedef struct
{
    HistoryEvent event;
    uint32_t crc;
} HistoryPush;
typedef struct
{
    uint32_t serial, id, eventCrc;
} HistoryAck;
typedef struct
{
    HistoryEvent event;
    uint16_t owner, synced;
    uint32_t crc, commit;
} HistoryRecord;
typedef struct
{
    uint32_t serial, id;
    uint16_t owner, reserved;
} HistoryCursor;
#define HJ_HEADER_SIZE (HJ_REMOTE ? 256u : 32u)
#define HJ_PAGES (HJ_SIZE / HJ_PAGE_SIZE)
#define HJ_PER_PAGE ((HJ_PAGE_SIZE - HJ_HEADER_SIZE) / 32u)
#define HJ_SLOTS (HJ_PAGES * HJ_PER_PAGE)
extern uint32_t historyStoreErrors, historyStoreFull;
void HJ_Init(void);
uint32_t HJ_Crc(const void *data, uint32_t size);
const HistoryRecord *HJ_Get(uint16_t slot);
uint8_t HJ_Append(HistoryEvent *event, uint16_t owner);
uint8_t HJ_Contains(const HistoryEvent *event, uint16_t owner);
const HistoryRecord *HJ_Pending(void);
uint8_t HJ_MarkSynced(uint32_t serial, uint32_t id);
#endif
