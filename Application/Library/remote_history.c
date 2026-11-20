#include "remote_history.h"
#include "main.h"
#include "define.h"
#include "cmsis_os.h"
#include "linkMonitor.h"
#include "utility.h"
#include <string.h>
extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];
extern osMessageQueueId_t txMsgQueueHandle;
typedef struct {
    HistoryEvent event;
    uint16_t transaction, owner;
    volatile uint8_t ready;
} PendingHistory;
static PendingHistory pending[4];
uint32_t remoteHistoryStored,remoteHistoryDuplicates,remoteHistoryAcked,remoteHistoryRejected,remoteHistoryRevision;
uint16_t RemoteHistory_Owner(uint8_t id)
{
    return ((uint16_t)systemInfo.netId<<8)|deviceBuff[id].nodeId;
}
void RemoteHistory_Init(void) { HJ_Init(); memset(pending,0,sizeof(pending)); }
static void enqueue(const HistoryEvent *event,uint16_t owner,uint16_t transaction)
{
    taskENTER_CRITICAL();
    for (uint8_t i=0;i<4;i++) if (!pending[i].ready) {
        pending[i].event=*event; pending[i].owner=owner; pending[i].transaction=transaction;
        pending[i].ready=1;
        taskEXIT_CRITICAL();
        return;
    }
    taskEXIT_CRITICAL();
    remoteHistoryRejected++;
}
void RemoteHistory_Handle(LoraPackage_t packet)
{
    if (packet.type!=LORA_PACKAGE_TYPE_REQUEST || packet.netId!=systemInfo.netId ||
        packet.byteCount!=LORA_PACKAGE_HEADER_SIZE+sizeof(HistoryPush)+1) { remoteHistoryRejected++; return; }
    HistoryPush push;
    memcpy(&push,packet.rawData+LORA_PACKAGE_HEADER_SIZE,sizeof(push));
    if (push.crc!=HJ_Crc(&push.event,sizeof(push.event))) { remoteHistoryRejected++; return; }
    HistoryEvent event;
    event=push.event;
    if (!event.id || event.id==0xFFFFFFFFu || event.action<HIST_ON || event.action>HIST_CLEAR ||
        event.source<HIST_REMOTE || event.source>HIST_SLAVE || event.before>2 || event.after>2 ||
        event.result<HIST_APPLIED || event.result>HIST_WARNING || (event.flags&~HIST_TIME_UNKNOWN)) {
        remoteHistoryRejected++; return;
    }
    for (uint8_t id=0;id<MAXIMUM_DEVICE;id++) {
        if (deviceBuff[id].isDelete || deviceBuff[id].nodeId!=packet.nodeId ||
            deviceBuff[id].factoryInfo.serial!=event.serial) continue;
        Link_RecordPacket(id,0);
        enqueue(&event,RemoteHistory_Owner(id),packet.transactionID);
        return;
    }
    remoteHistoryRejected++;
}
uint8_t RemoteHistory_Busy(void)
{
    for (uint8_t i=0;i<4;i++) if (pending[i].ready) return 1;
    return 0;
}
void RemoteHistory_Link(uint8_t id,uint8_t restored)
{
    if (id>=MAXIMUM_DEVICE || deviceBuff[id].isDelete) return;
    HistoryEvent event={0};
    event.serial=deviceBuff[id].factoryInfo.serial; event.timestamp=systemInfo.currentTime;
    event.action=restored?HIST_LINK_RESTORED:HIST_LINK_LOST; event.source=HIST_MONITOR;
    /* Link detection is not motor feedback: never present stale ON/OFF as a fact. */
    event.before=event.after=MOTOR_POWER_IDLE;
    event.result=restored?HIST_APPLIED:HIST_WARNING;
    event.error=restored?0:HIST_ERROR_NO_STATUS; event.flags=HIST_LOCAL;
    if (event.timestamp<1704067200u) event.flags|=HIST_TIME_UNKNOWN;
    enqueue(&event,RemoteHistory_Owner(id),0);
}
void RemoteHistory_Tick(void)
{
    for (uint8_t i=0;i<4;i++) {
        if (pending[i].ready!=1) continue;
        pending[i].ready=2;
        PendingHistory *p=&pending[i];
        uint8_t existing=p->event.id?HJ_Contains(&p->event,p->owner):0;
        if (existing==2 || (!existing && !HJ_Append(&p->event,p->owner))) {
            remoteHistoryRejected++; p->ready=0; continue;
        }
        if (existing) remoteHistoryDuplicates++;
        else { remoteHistoryStored++; remoteHistoryRevision++; }
        if (!(p->event.flags&HIST_LOCAL)) {
            HistoryAck ack={p->event.serial,p->event.id,HJ_Crc(&p->event,sizeof(p->event))};
            LoraPackage_t packet=createNewPackage(p->owner>>8,p->owner&255,CMD_HISTORY,
                LORA_PACKAGE_TYPE_RESPONSE,(uint8_t *)&ack,sizeof(ack),0);
            packet.transactionID=p->transaction;
            memcpy(packet.rawData+6,&packet.transactionID,2);
            packet.checkSum=calculateCRC(packet.rawData,packet.length);
            packet.rawData[packet.byteCount-1]=packet.checkSum;
            if (osMessageQueuePut(txMsgQueueHandle,&packet,0,0)==osOK) remoteHistoryAcked++;
        }
        p->ready=0;
    }
}
