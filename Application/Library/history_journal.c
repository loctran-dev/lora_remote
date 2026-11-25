#include "history_journal.h"
#include "stm32f1xx_hal.h"
#include <string.h>

#define PAGE_MAGIC   0x314A5048u
#define RECORD_MAGIC 0x314A5248u
#define CURSORS      20

/* Header at the start of each journal page; commit is written last. */
typedef struct
{
    uint32_t generation;
    uint32_t lastId;
#if HJ_REMOTE
    HistoryCursor cursors[CURSORS];
#else
    uint32_t reserved[4];
#endif
    uint32_t crc;
    uint32_t commit;
} HJ_PageHeader_t;

typedef char HJ_CheckHeaderSize_t[(sizeof(HJ_PageHeader_t) == HJ_HEADER_SIZE) ? 1 : -1];
typedef char HJ_CheckRecordSize_t[(sizeof(HistoryRecord) == 32) ? 1 : -1];

uint32_t historyStoreErrors;
uint32_t historyStoreFull;

static uint32_t generation;
static uint32_t lastId;
static int16_t activePage;
static uint16_t nextSlot;
static uint8_t pageGood[HJ_PAGES];
#if HJ_REMOTE
static HistoryCursor cursors[CURSORS];
#endif

static const HJ_PageHeader_t *page(uint16_t p);
static uint8_t pageValid(uint16_t p);
static const HistoryRecord *record(uint16_t p, uint16_t slot);
static uint8_t blank(const void *data, uint32_t size);
static uint8_t valid(const HistoryRecord *r);
static uint8_t program(uint32_t address, const void *data, uint16_t size);
static uint8_t newPage(void);
#if HJ_REMOTE
static HistoryCursor *cursor(uint16_t owner, uint32_t serial, uint8_t create);
static void remember(const HistoryRecord *r);
#endif

/* CRC ====================================================================== */

/**
 * @brief  CRC-32 (IEEE 802.3, reflected) of a buffer.
 * @param  data  Bytes to check.
 * @param  size  Number of bytes.
 * @return The CRC.
 */
uint32_t HJ_Crc(const void *data, uint32_t size)
{
    const uint8_t *p = data;
    uint32_t crc = 0xFFFFFFFFu;

    while (size--)
    {
        crc ^= *p++;
        for (uint8_t b = 0; b < 8; b++)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

/* Initialisation =========================================================== */

/**
 * @brief  Scan the journal pages and rebuild the RAM state.
 *
 * Finds the valid pages, the active page and its next free slot, the highest
 * event id and, on the remote, the per-owner cursors.
 *
 * @note   Call once at start-up (RemoteHistory_Init()).
 */
void HJ_Init(void)
{
    const HistoryRecord *r;
#if HJ_REMOTE
    HistoryCursor *c;
#endif

    activePage = -1;
    generation = lastId = nextSlot = 0;
    historyStoreErrors = historyStoreFull = 0;
#if HJ_REMOTE
    memset(cursors, 0, sizeof(cursors));
#endif

    /* Find valid pages, the newest page and the highest event ID. */
    for (uint16_t p = 0; p < HJ_PAGES; p++)
    {
        pageGood[p] = pageValid(p);
    }
    for (uint16_t p = 0; p < HJ_PAGES; p++)
    {
        if (!pageGood[p])
        {
            continue;
        }
        if (page(p)->lastId > lastId)
        {
            lastId = page(p)->lastId;
        }
        if (activePage < 0 || page(p)->generation > generation)
        {
            activePage = p;
            generation = page(p)->generation;
        }
    }
#if HJ_REMOTE
    if (activePage >= 0)
    {
        memcpy(cursors, page(activePage)->cursors, sizeof(cursors));
    }
#endif

    /* Scan records: next free slot, last ID and per-owner cursors. */
    for (uint16_t p = 0; p < HJ_PAGES; p++)
    {
        if (!pageGood[p])
        {
            continue;
        }
        for (uint16_t i = 0; i < HJ_PER_PAGE; i++)
        {
            r = record(p, i);
            if (p == activePage && !blank(r, sizeof(*r)))
            {
                nextSlot = i + 1;
            }
            if (!valid(r))
            {
                continue;
            }
            if (r->event.id > lastId)
            {
                lastId = r->event.id;
            }
#if HJ_REMOTE
            /* Only advance a known snapshot; old pages cannot replace a newer owner. */
            c = cursor(r->owner, r->event.serial, 0);
            if (!(r->event.flags & HIST_LOCAL) && c && c->id < r->event.id)
            {
                c->id = r->event.id;
            }
#endif
        }
    }

#if HJ_REMOTE
    /* The active page may contain identities first seen after its header commit. */
    if (activePage >= 0)
    {
        for (uint16_t i = 0; i < HJ_PER_PAGE; i++)
        {
            r = record(activePage, i);
            if (valid(r))
            {
                remember(r);
            }
        }
    }
#endif
}

/* Record access ============================================================ */

/**
 * @brief  Read one record.
 * @param  slot  0 .. HJ_SLOTS-1, in flash order (not time order).
 * @return The record in flash, or 0 if the slot is empty or invalid.
 */
const HistoryRecord *HJ_Get(uint16_t slot)
{
    const HistoryRecord *r;

    if (slot >= HJ_SLOTS || !pageGood[slot / HJ_PER_PAGE])
    {
        return 0;
    }
    r = record(slot / HJ_PER_PAGE, slot % HJ_PER_PAGE);
    return valid(r) ? r : 0;
}

/**
 * @brief  Check whether an event from a device is already stored.
 * @param  event  Event with its id set.
 * @param  owner  Owner of the device.
 * @return 0 = not stored, 1 = stored with the same content (or already
 *         stored and overwritten since), 2 = stored with different content.
 */
uint8_t HJ_Contains(const HistoryEvent *event, uint16_t owner)
{
    const HistoryRecord *r;
#if HJ_REMOTE
    HistoryCursor *c;
#endif

    for (uint16_t s = 0; s < HJ_SLOTS; s++)
    {
        if (!pageGood[s / HJ_PER_PAGE])
        {
            continue;
        }
        r = record(s / HJ_PER_PAGE, s % HJ_PER_PAGE);
        if (r->owner == owner && r->event.serial == event->serial &&
            r->event.id == event->id && ((r->event.flags ^ event->flags) & HIST_LOCAL) == 0)
        {
            if (valid(r))
            {
                return memcmp(&r->event, event, sizeof(*event)) ? 2 : 1;
            }
        }
    }
#if HJ_REMOTE
    if (!(event->flags & HIST_LOCAL))
    {
        c = cursor(owner, event->serial, 0);
        if (c && event->id <= c->id)
        {
            return 1; /* Retention expired, already stored before. */
        }
    }
#endif
    return 0;
}

/**
 * @brief  Store an event, opening a new page when the active one is full.
 * @param  event  Event to store; an id of 0 is replaced by the next free id.
 * @param  owner  Owner of the device.
 * @return 1 if stored or an identical copy exists, 0 on a flash error, a
 *         conflicting copy or an exhausted id space.
 * @warning Opening a new page erases the oldest page of the ring.
 */
uint8_t HJ_Append(HistoryEvent *event, uint16_t owner)
{
    uint8_t found;
    HistoryRecord r;
    uint32_t address;
    uint8_t ok;

    /* Assign a new ID, or skip an event that is already stored. */
    if (!event->id)
    {
        if (lastId >= 0xFFFFFFFEu)
        {
            historyStoreFull++;
            return 0;
        }
        event->id = lastId + 1;
    }
    else
    {
        found = HJ_Contains(event, owner);
        if (found)
        {
            return found == 1;
        }
    }
    if (activePage < 0 || nextSlot >= HJ_PER_PAGE)
    {
        if (!newPage())
        {
            return 0;
        }
    }

    /* Program the record; commit is written last. */
    r.event = *event;
    r.owner = owner;
    r.synced = 0xFFFFu;
    r.crc = HJ_Crc(&r, 22);
    r.commit = RECORD_MAGIC;
    address = (uint32_t)record(activePage, nextSlot++);
    HAL_FLASH_Unlock();
    ok = program(address, &r, 28);
    if (ok)
    {
        ok = program(address + 28, &r.commit, 4);
    }
    HAL_FLASH_Lock();
    if (!ok)
    {
        historyStoreErrors++;
        return 0;
    }

    if (event->id > lastId)
    {
        lastId = event->id;
    }
#if HJ_REMOTE
    remember((const HistoryRecord *)address);
#endif
    return 1;
}

/* Synchronisation ========================================================== */

/**
 * @brief  Oldest record not yet marked as synced.
 * @return The record, or 0 if every record is synced.
 */
const HistoryRecord *HJ_Pending(void)
{
    const HistoryRecord *found = 0;
    const HistoryRecord *r;

    for (uint16_t s = 0; s < HJ_SLOTS; s++)
    {
        if (!pageGood[s / HJ_PER_PAGE])
        {
            continue;
        }
        r = record(s / HJ_PER_PAGE, s % HJ_PER_PAGE);
        if (r->synced != 0 && (!found || r->event.id < found->event.id) && valid(r))
        {
            found = r;
        }
    }
    return found;
}

/**
 * @brief  Mark a record as synced by programming its synced field to 0.
 * @param  serial  Event serial.
 * @param  id      Event id.
 * @return 1 if the record is now marked, 0 if it was not found or the flash write failed.
 */
uint8_t HJ_MarkSynced(uint32_t serial, uint32_t id)
{
    const HistoryRecord *r;
    uint8_t ok;

    for (uint16_t s = 0; s < HJ_SLOTS; s++)
    {
        r = HJ_Get(s);
        if (!r || r->event.serial != serial || r->event.id != id)
        {
            continue;
        }
        if (!r->synced)
        {
            return 1;
        }
        HAL_FLASH_Unlock();
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (uint32_t)&r->synced, 0) == HAL_OK;
        HAL_FLASH_Lock();
        if (!ok)
        {
            historyStoreErrors++;
        }
        return ok && !r->synced;
    }
    return 0;
}

/* Private helpers ========================================================== */

static const HJ_PageHeader_t *page(uint16_t p)
{
    return (const HJ_PageHeader_t *)(HJ_BASE + p * HJ_PAGE_SIZE);
}

static uint8_t pageValid(uint16_t p)
{
    const HJ_PageHeader_t *h = page(p);

    return h->commit == PAGE_MAGIC && HJ_Crc(h, sizeof(*h) - 8) == h->crc;
}

static const HistoryRecord *record(uint16_t p, uint16_t slot)
{
    return (const HistoryRecord *)(HJ_BASE + p * HJ_PAGE_SIZE + HJ_HEADER_SIZE + slot * 32u);
}

static uint8_t blank(const void *data, uint32_t size)
{
    const uint32_t *p = data;

    while (size)
    {
        if (*p++ != 0xFFFFFFFFu)
        {
            return 0;
        }
        size -= 4;
    }
    return 1;
}

static uint8_t valid(const HistoryRecord *r)
{
    return r->commit == RECORD_MAGIC && r->event.id && r->event.id != 0xFFFFFFFFu &&
        HJ_Crc(r, 22) == r->crc;
}

static uint8_t program(uint32_t address, const void *data, uint16_t size)
{
    const uint32_t *words = data;

    for (uint16_t i = 0; i < size / 4; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i * 4, words[i]) != HAL_OK)
        {
            return 0;
        }
    }
    return !memcmp((const void *)address, data, size);
}

static uint8_t newPage(void)
{
    uint16_t p = activePage < 0 ? 0 : (activePage + 1) % HJ_PAGES;
    uint32_t error;
    HJ_PageHeader_t h;
    uint8_t ok = 1;
    FLASH_EraseInitTypeDef erase = {0};
#if !HJ_REMOTE
    const HistoryRecord *r;

    /* The slave must not overwrite records the remote has not received yet. */
    if (pageGood[p])
    {
        for (uint16_t i = 0; i < HJ_PER_PAGE; i++)
        {
            r = record(p, i);
            if (valid(r) && r->synced != 0)
            {
                historyStoreFull++;
                return 0;
            }
        }
    }
#endif

    /* Build the page header. */
    memset(&h, 0, sizeof(h));
    h.generation = generation + 1;
    h.lastId = lastId;
#if HJ_REMOTE
    memcpy(h.cursors, cursors, sizeof(cursors));
#endif
    h.crc = HJ_Crc(&h, sizeof(h) - 8);
    h.commit = PAGE_MAGIC;

    /* Erase if needed, then program the header with commit written last. */
    HAL_FLASH_Unlock();
    pageGood[p] = 0;
    if (!blank(page(p), HJ_PAGE_SIZE))
    {
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.PageAddress = HJ_BASE + p * HJ_PAGE_SIZE;
        erase.NbPages = 1;
        ok = HAL_FLASHEx_Erase(&erase, &error) == HAL_OK;
    }
    if (ok)
    {
        ok = program((uint32_t)page(p), &h, sizeof(h) - 4);
    }
    if (ok)
    {
        ok = program((uint32_t)page(p) + sizeof(h) - 4, &h.commit, 4);
    }
    HAL_FLASH_Lock();
    if (!ok)
    {
        historyStoreErrors++;
        return 0;
    }

    activePage = p;
    generation = h.generation;
    nextSlot = 0;
    pageGood[p] = 1;
    return 1;
}

#if HJ_REMOTE
static HistoryCursor *cursor(uint16_t owner, uint32_t serial, uint8_t create)
{
    HistoryCursor *replacement = 0;

    for (uint8_t i = 0; i < CURSORS; i++)
    {
        if (cursors[i].owner == owner && cursors[i].serial == serial)
        {
            return &cursors[i];
        }
        if (!cursors[i].owner || cursors[i].owner == owner)
        {
            replacement = &cursors[i];
        }
    }
    if (!create)
    {
        return 0;
    }
    if (!replacement)
    {
        replacement = &cursors[0];
    }
    replacement->owner = owner;
    replacement->serial = serial;
    replacement->id = 0;
    return replacement;
}

static void remember(const HistoryRecord *r)
{
    HistoryCursor *c;

    if (r->event.flags & HIST_LOCAL)
    {
        return;
    }
    c = cursor(r->owner, r->event.serial, 1);
    if (c->id < r->event.id)
    {
        c->id = r->event.id;
    }
}
#endif
