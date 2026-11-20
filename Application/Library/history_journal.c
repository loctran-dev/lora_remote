#include "history_journal.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#define PAGE_MAGIC 0x314A5048u
#define RECORD_MAGIC 0x314A5248u
typedef struct {
    uint32_t generation, lastId;
#if HJ_REMOTE
    HistoryCursor cursors[20];
#else
    uint32_t reserved[4];
#endif
    uint32_t crc, commit;
} PageHeader;
typedef char CheckHeaderSize[(sizeof(PageHeader)==HJ_HEADER_SIZE)?1:-1];
typedef char CheckRecordSize[(sizeof(HistoryRecord)==32)?1:-1];
static uint32_t generation, lastId;
static int16_t activePage;
static uint16_t nextSlot;
static uint8_t pageGood[HJ_PAGES];
#if HJ_REMOTE
static HistoryCursor cursors[20];
#endif
uint32_t historyStoreErrors, historyStoreFull;

uint32_t HJ_Crc(const void *data, uint32_t size)
{
    const uint8_t *p = data;
    uint32_t crc = 0xFFFFFFFFu;
    while (size--) {
        crc ^= *p++;
        for (uint8_t b=0; b<8; b++) crc = (crc>>1) ^ (0xEDB88320u & (0u-(crc&1u)));
    }
    return ~crc;
}
static const PageHeader *page(uint16_t p) { return (const PageHeader *)(HJ_BASE + p*HJ_PAGE_SIZE); }
static uint8_t pageValid(uint16_t p)
{
    const PageHeader *h=page(p);
    return h->commit==PAGE_MAGIC && HJ_Crc(h,sizeof(*h)-8)==h->crc;
}
static const HistoryRecord *record(uint16_t p, uint16_t slot)
{
    return (const HistoryRecord *)(HJ_BASE+p*HJ_PAGE_SIZE+HJ_HEADER_SIZE+slot*32u);
}
static uint8_t blank(const void *data, uint32_t size)
{
    const uint32_t *p=data;
    while (size) { if (*p++ != 0xFFFFFFFFu) return 0; size-=4; }
    return 1;
}
static uint8_t valid(const HistoryRecord *r)
{
    return r->commit==RECORD_MAGIC && r->event.id && r->event.id!=0xFFFFFFFFu &&
        HJ_Crc(r,22)==r->crc;
}
const HistoryRecord *HJ_Get(uint16_t slot)
{
    if (slot>=HJ_SLOTS || !pageGood[slot/HJ_PER_PAGE]) return 0;
    const HistoryRecord *r=record(slot/HJ_PER_PAGE,slot%HJ_PER_PAGE);
    return valid(r)?r:0;
}
#if HJ_REMOTE
static HistoryCursor *cursor(uint16_t owner, uint32_t serial, uint8_t create)
{
    HistoryCursor *replacement=0;
    for (uint8_t i=0;i<20;i++) {
        if (cursors[i].owner==owner && cursors[i].serial==serial) return &cursors[i];
        if (!cursors[i].owner || cursors[i].owner==owner) replacement=&cursors[i];
    }
    if (!create) return 0;
    if (!replacement) replacement=&cursors[0];
    replacement->owner=owner; replacement->serial=serial; replacement->id=0;
    return replacement;
}
static void remember(const HistoryRecord *r)
{
    if (r->event.flags&HIST_LOCAL) return;
    HistoryCursor *c=cursor(r->owner,r->event.serial,1);
    if (c->id<r->event.id) c->id=r->event.id;
}
#endif
void HJ_Init(void)
{
    activePage=-1; generation=lastId=nextSlot=0;
    historyStoreErrors=historyStoreFull=0;
#if HJ_REMOTE
    memset(cursors,0,sizeof(cursors));
#endif
    for (uint16_t p=0;p<HJ_PAGES;p++) pageGood[p]=pageValid(p);
    for (uint16_t p=0;p<HJ_PAGES;p++) if (pageGood[p]) {
        if (page(p)->lastId>lastId) lastId=page(p)->lastId;
        if (activePage<0 || page(p)->generation>generation) {
            activePage=p; generation=page(p)->generation;
        }
    }
#if HJ_REMOTE
    if (activePage>=0) memcpy(cursors,page(activePage)->cursors,sizeof(cursors));
#endif
    for (uint16_t p=0;p<HJ_PAGES;p++) if (pageGood[p]) {
        for (uint16_t i=0;i<HJ_PER_PAGE;i++) {
            const HistoryRecord *r=record(p,i);
            if (p==activePage && !blank(r,sizeof(*r))) nextSlot=i+1;
            if (!valid(r)) continue;
            if (r->event.id>lastId) lastId=r->event.id;
#if HJ_REMOTE
            /* Only advance a known snapshot; old pages cannot replace a newer owner. */
            HistoryCursor *c=cursor(r->owner,r->event.serial,0);
            if (!(r->event.flags&HIST_LOCAL) && c && c->id<r->event.id) c->id=r->event.id;
#endif
        }
    }
#if HJ_REMOTE
    /* The active page may contain identities first seen after its header commit. */
    if (activePage>=0) for (uint16_t i=0;i<HJ_PER_PAGE;i++) {
        const HistoryRecord *r=record(activePage,i);
        if (valid(r)) remember(r);
    }
#endif
}
static uint8_t program(uint32_t address, const void *data, uint16_t size)
{
    const uint32_t *words=data;
    for (uint16_t i=0;i<size/4;i++)
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,address+i*4,words[i])!=HAL_OK) return 0;
    return !memcmp((const void *)address,data,size);
}
static uint8_t newPage(void)
{
    uint16_t p=activePage<0?0:(activePage+1)%HJ_PAGES;
#if !HJ_REMOTE
    if (pageGood[p]) for (uint16_t i=0;i<HJ_PER_PAGE;i++) {
        const HistoryRecord *r=record(p,i);
        if (valid(r) && r->synced!=0) { historyStoreFull++; return 0; }
    }
#endif
    uint32_t error;
    PageHeader h;
    memset(&h,0,sizeof(h));
    h.generation=generation+1; h.lastId=lastId;
#if HJ_REMOTE
    memcpy(h.cursors,cursors,sizeof(cursors));
#endif
    h.crc=HJ_Crc(&h,sizeof(h)-8); h.commit=PAGE_MAGIC;
    HAL_FLASH_Unlock();
    pageGood[p]=0;
    uint8_t ok=1;
    if (!blank(page(p),HJ_PAGE_SIZE)) {
        FLASH_EraseInitTypeDef erase={0};
        erase.TypeErase=FLASH_TYPEERASE_PAGES; erase.PageAddress=HJ_BASE+p*HJ_PAGE_SIZE; erase.NbPages=1;
        ok=HAL_FLASHEx_Erase(&erase,&error)==HAL_OK;
    }
    if (ok) ok=program((uint32_t)page(p),&h,sizeof(h)-4);
    if (ok) ok=program((uint32_t)page(p)+sizeof(h)-4,&h.commit,4);
    HAL_FLASH_Lock();
    if (!ok) { historyStoreErrors++; return 0; }
    activePage=p; generation=h.generation; nextSlot=0;
    pageGood[p]=1;
    return 1;
}
uint8_t HJ_Contains(const HistoryEvent *event, uint16_t owner)
{
    for (uint16_t s=0;s<HJ_SLOTS;s++) {
        if (!pageGood[s/HJ_PER_PAGE]) continue;
        const HistoryRecord *r=record(s/HJ_PER_PAGE,s%HJ_PER_PAGE);
        if (r->owner==owner && r->event.serial==event->serial &&
            r->event.id==event->id && ((r->event.flags^event->flags)&HIST_LOCAL)==0)
            if (valid(r)) return memcmp(&r->event,event,sizeof(*event))?2:1;
    }
#if HJ_REMOTE
    if (!(event->flags&HIST_LOCAL)) {
        HistoryCursor *c=cursor(owner,event->serial,0);
        if (c && event->id<=c->id) return 1; /* Retention expired, already stored before. */
    }
#endif
    return 0;
}
uint8_t HJ_Append(HistoryEvent *event, uint16_t owner)
{
    if (!event->id) {
        if (lastId>=0xFFFFFFFEu) { historyStoreFull++; return 0; }
        event->id=lastId+1;
    } else {
        uint8_t found=HJ_Contains(event,owner);
        if (found) return found==1;
    }
    if (activePage<0 || nextSlot>=HJ_PER_PAGE) if (!newPage()) return 0;
    HistoryRecord r;
    r.event=*event; r.owner=owner; r.synced=0xFFFFu;
    r.crc=HJ_Crc(&r,22); r.commit=RECORD_MAGIC;
    uint32_t address=(uint32_t)record(activePage,nextSlot++);
    HAL_FLASH_Unlock();
    uint8_t ok=program(address,&r,28);
    if (ok) ok=program(address+28,&r.commit,4);
    HAL_FLASH_Lock();
    if (!ok) { historyStoreErrors++; return 0; }
    if (event->id>lastId) lastId=event->id;
#if HJ_REMOTE
    remember((const HistoryRecord *)address);
#endif
    return 1;
}
const HistoryRecord *HJ_Pending(void)
{
    const HistoryRecord *found=0;
    for (uint16_t s=0;s<HJ_SLOTS;s++) {
        if (!pageGood[s/HJ_PER_PAGE]) continue;
        const HistoryRecord *r=record(s/HJ_PER_PAGE,s%HJ_PER_PAGE);
        if (r->synced!=0 && (!found || r->event.id<found->event.id) && valid(r)) found=r;
    }
    return found;
}
uint8_t HJ_MarkSynced(uint32_t serial, uint32_t id)
{
    for (uint16_t s=0;s<HJ_SLOTS;s++) {
        const HistoryRecord *r=HJ_Get(s);
        if (!r || r->event.serial!=serial || r->event.id!=id) continue;
        if (!r->synced) return 1;
        HAL_FLASH_Unlock();
        uint8_t ok=HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,(uint32_t)&r->synced,0)==HAL_OK;
        HAL_FLASH_Lock();
        if (!ok) historyStoreErrors++;
        return ok && !r->synced;
    }
    return 0;
}
