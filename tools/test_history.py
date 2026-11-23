"""Built-ARM history journal checks. HAL Flash is emulated; no board access."""
import os, re, struct, subprocess, unittest, zlib
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_PC, UC_ARM_REG_LR, UC_ARM_REG_SP
ROOT=Path(__file__).resolve().parents[1]
REMOTE=ROOT.name.lower()=="lora_remote"
TARGET="USB_Application" if REMOTE else "Motor_USB_Application"
BUILD=ROOT/"Application/MDK-ARM"/TARGET
MAP=(BUILD/(TARGET+".map")).read_text()
FROMELF=Path(os.environ["LOCALAPPDATA"])/"Keil_v5/ARM/ARM_Compiler_5.06u7/bin/fromelf.exe"
BASE=0x0803A000 if REMOTE else 0x0800DC00
PAGE=2048 if REMOTE else 1024
HEADER=256 if REMOTE else 32
PER=(PAGE-HEADER)//32
SLOTS=PER*8
RAW=18 if REMOTE else 14
def symbol(name):
    # "file.o:name" selects a static symbol that exists in several objects.
    obj,_,sym=name.rpartition(":")
    owner=r"\s+"+re.escape(obj)+r"\(" if obj else ""
    m=re.search(r"^\s+"+re.escape(sym)+r"\s+(0x[0-9a-fA-F]+)\s+(?:Data|Thumb Code)\s+(\d+)"+owner,MAP,re.M)
    assert m,name
    return int(m[1],16)
class JournalTests(unittest.TestCase):
    def setUp(self):
        self.cpu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        self.cpu.mem_map(0x08000000,0x80000)
        self.cpu.mem_write(0x08000000,b"\xff"*0x80000)
        self.cpu.mem_write(0x08008000 if REMOTE else 0x08000000,(ROOT/"dist/application.bin").read_bytes())
        self.cpu.mem_map(0x20000000,0x10000)
        self.programs=0; self.fail_at=None; self.fail_erase=False; self.erases=[]; self.queued=[]
        self.locked=True
        names=["HAL_FLASH_Unlock","HAL_FLASH_Lock","HAL_FLASH_Program","HAL_FLASHEx_Erase",
               "osMutexAcquire","osMutexRelease","osMessageQueuePut","vPortEnterCritical","vPortExitCritical",
               "Link_RecordPacket","HAL_GPIO_WritePin"]
        self.hooks={}
        for n in names:
            try: self.hooks[symbol(n)&~1]=n
            except AssertionError: pass
        def hook(cpu,address,size,user):
            if address not in self.hooks:return
            n=self.hooks[address]; result=0
            if n=="HAL_FLASH_Unlock":self.locked=False
            elif n=="HAL_FLASH_Lock":self.locked=True
            elif n=="HAL_FLASH_Program":
                self.assertFalse(self.locked)
                self.programs+=1
                kind=cpu.reg_read(UC_ARM_REG_R0); address=cpu.reg_read(UC_ARM_REG_R1)
                length=2 if kind==1 else 4
                self.assertTrue(BASE<=address<=BASE+PAGE*8-length)
                result=int(self.programs==self.fail_at)
                if not result:
                    value=cpu.reg_read(UC_ARM_REG_R2).to_bytes(4,"little")[:length]
                    old=bytes(cpu.mem_read(address,length))
                    self.assertTrue(all((a&b)==b for a,b in zip(old,value)))
                    cpu.mem_write(address,value)
            elif n=="HAL_FLASHEx_Erase":
                self.assertFalse(self.locked)
                ptr=cpu.reg_read(UC_ARM_REG_R0)
                address,count=struct.unpack("<II",bytes(cpu.mem_read(ptr+8,8)))
                self.assertEqual(count,1);self.assertTrue(BASE<=address<BASE+PAGE*8)
                self.assertEqual((address-BASE)%PAGE,0)
                result=int(self.fail_erase)
                if not result:self.erases.append(address);cpu.mem_write(address,b"\xff"*PAGE)
            elif n=="osMessageQueuePut":
                p=bytes(cpu.mem_read(cpu.reg_read(UC_ARM_REG_R1),RAW+256))
                length=int.from_bytes(p[10:14],"little")
                self.queued.append(p[RAW:RAW+length])
            cpu.reg_write(UC_ARM_REG_R0,result)
            cpu.reg_write(UC_ARM_REG_PC,cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE,hook)
        self.call("HJ_Init")
    def call(self,name,*args):
        for reg,value in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3],args):
            self.cpu.reg_write(reg,value&0xffffffff)
        self.cpu.reg_write(UC_ARM_REG_SP,0x2000f000);self.cpu.reg_write(UC_ARM_REG_LR,0x0807fff1)
        self.cpu.emu_start(symbol(name)|1,0x0807fff0,count=5000000)
        self.assertEqual(self.cpu.reg_read(UC_ARM_REG_PC),0x0807fff0,name)
        return self.cpu.reg_read(UC_ARM_REG_R0)
    def append(self,id=0,owner=0x101,serial=0xabcdef01,action=1,flags=0):
        self.cpu.mem_write(0x20008000,struct.pack("<IIIH6B",serial,id,1794967200,0,action,1,2,1,1,flags))
        result=self.call("HJ_Append",0x20008000,owner)
        event=bytes(self.cpu.mem_read(0x20008000,20))
        return result,int.from_bytes(event[4:8],"little"),event
    def records(self):
        result=[]
        for s in range(SLOTS):
            p=self.call("HJ_Get",s)
            if p:result.append(bytes(self.cpu.mem_read(p,32)))
        return result
    def test_durable_commit_and_id_survive_reinit(self):
        self.assertEqual(self.append()[:2],(1,1))
        self.call("HJ_Init")
        self.assertEqual(self.append()[:2],(1,2))
        self.assertEqual(len(self.records()),2)
        self.assertTrue(self.locked)
    def test_crc_rejects_corrupted_record(self):
        self.append()
        address=self.call("HJ_Get",0)
        self.cpu.mem_write(address+8,b"\x00\x00\x00\x00")
        self.call("HJ_Init")
        self.assertFalse(self.call("HJ_Get",0))
    def test_torn_record_commit_is_not_visible_or_acked(self):
        self.append();self.fail_at=self.programs+8
        self.assertEqual(self.append()[0],0)
        self.call("HJ_Init")
        self.assertEqual(len(self.records()),1)
        self.assertEqual(self.append()[:2],(1,2))
    def test_duplicate_id_does_not_write_and_conflicting_id_is_rejected(self):
        self.append(42)
        programs=self.programs
        self.assertEqual(self.append(42)[0],1)
        self.assertEqual(self.programs,programs)
        self.assertEqual(self.append(42,action=2)[0],0)
        self.assertEqual(self.programs,programs)
    def test_same_id_from_different_owner_is_distinct(self):
        self.append(1,owner=0x101);self.append(1,owner=0x102)
        self.assertEqual(len(self.records()),2)
    @unittest.skipIf(REMOTE,"slave-only sync marks")
    def test_wrong_ack_id_does_not_modify_storage(self):
        self.append()
        self.assertFalse(self.call("HJ_MarkSynced",0xabcdef01,999))
        self.assertNotEqual(self.call("HJ_Pending"),0)
    @unittest.skipIf(REMOTE,"slave-only sync marks")
    def test_ack_is_durable_and_does_not_invalidate_crc(self):
        self.append();self.append()
        self.assertEqual(self.call("HJ_MarkSynced",0xabcdef01,1),1)
        self.call("HJ_Init")
        p=self.call("HJ_Pending")
        self.assertEqual(int.from_bytes(self.cpu.mem_read(p+4,4),"little"),2)
        self.assertEqual(len(self.records()),2)
    @unittest.skipIf(REMOTE,"slave-only backpressure")
    def test_full_outbox_never_erases_unsynced_events(self):
        for _ in range(SLOTS):self.assertEqual(self.append()[0],1)
        self.assertFalse(self.append()[0])
        self.assertEqual(self.erases,[])
        self.assertEqual(int.from_bytes(self.cpu.mem_read(self.call("HJ_Pending")+4,4),"little"),1)
    @unittest.skipIf(REMOTE,"slave-only reclaim")
    def test_synced_page_can_be_recycled_without_reusing_ids(self):
        for _ in range(SLOTS):
            ok,id,_=self.append();self.assertTrue(ok)
            self.assertTrue(self.call("HJ_MarkSynced",0xabcdef01,id))
        self.assertEqual(self.append()[:2],(1,SLOTS+1))
        self.assertEqual(self.erases,[BASE])
        self.call("HJ_Init")
        self.assertEqual(self.append()[:2],(1,SLOTS+2))
    @unittest.skipUnless(REMOTE,"remote-only cursor snapshots")
    def test_dedup_watermark_survives_retention_and_reboot(self):
        self.append(1)
        for i in range(2,SLOTS+PER+3):
            self.assertTrue(self.append(i,owner=0x102,serial=2)[0])
        self.call("HJ_Init")
        programs=self.programs
        self.assertEqual(self.append(1)[0],1)
        self.assertEqual(self.programs,programs)
    def test_flash_write_never_touches_configuration_or_app(self):
        self.append()
        if not REMOTE:self.call("HJ_MarkSynced",0xabcdef01,1)
        self.assertTrue(self.locked)
        self.assertTrue(all(BASE<=a<BASE+PAGE*8 for a in self.erases))

    def globals_for_sync(self):
        if REMOTE:
            self.call("RemoteHistory_Init")
            system=symbol("systemInfo"); device=symbol("deviceBuff")
            self.cpu.mem_write(system,b"\x01")
            self.cpu.mem_write(device+1,b"\x01")
            self.cpu.mem_write(device+4,struct.pack("<I",0xabcdef01))
        else:
            import test_persistence as old
            for field,value,size in [("netId",1,1),("nodeId",1,1),("sysState",2,1),
                                     ("powerState",2,1),("currentTime",1794967200,4)]:
                self.cpu.mem_write(symbol("sysInfo")+old.member(field),value.to_bytes(size,"little"))
            self.cpu.mem_write(symbol("factoryInfo"),struct.pack("<I",0xabcdef01))
            self.call("MotorHistory_Init")
    def tick_ms(self,value):
        self.cpu.mem_write(symbol("uwTick"),struct.pack("<I",value))
    def make_packet(self,wire):
        packet=bytearray(RAW+256);packet[:9]=wire[:9]
        packet[10:14]=struct.pack("<I",len(wire));packet[RAW:RAW+len(wire)]=wire
        return packet
    def packet_call(self,name,wire):
        packet=self.make_packet(wire)
        stack=0x2000e000;self.cpu.mem_write(stack,bytes(packet[16:]))
        for reg,value in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3],struct.unpack("<4I",packet[:16])):
            self.cpu.reg_write(reg,value)
        self.cpu.reg_write(UC_ARM_REG_SP,stack);self.cpu.reg_write(UC_ARM_REG_LR,0x0807fff1)
        self.cpu.emu_start(symbol(name)|1,0x0807fff0,count=5000000)
        self.assertEqual(self.cpu.reg_read(UC_ARM_REG_PC),0x0807fff0)
    def event_wire(self,serial=0xabcdef01,id=1):
        event=struct.pack("<IIIH6B",serial,id,1794967200,0,1,1,2,1,1,0)
        return struct.pack("<HBBBBHB",0xff55,31,1,1,1,42,9)+event+struct.pack("<I",zlib.crc32(event))+b"\x00"
    @unittest.skipUnless(REMOTE,"remote commit-before-ACK")
    def test_remote_only_acks_after_durable_commit_and_deduplicates_retries(self):
        self.globals_for_sync()
        wire=self.event_wire()
        self.packet_call("RemoteHistory_Handle",wire)
        self.assertEqual(self.queued,[])
        self.assertEqual(len(self.records()),0)
        self.call("RemoteHistory_Tick")
        self.assertEqual(len(self.records()),1);self.assertEqual(len(self.queued),1)
        ack=self.queued[-1]
        self.assertEqual(struct.unpack("<III",ack[9:21]),(0xabcdef01,1,zlib.crc32(wire[9:29])))
        self.call("RemoteHistory_Init")
        self.packet_call("RemoteHistory_Handle",wire);self.call("RemoteHistory_Tick")
        self.assertEqual(len(self.records()),1);self.assertEqual(len(self.queued),2)
    @unittest.skipUnless(REMOTE,"remote storage failure")
    def test_remote_never_acks_a_failed_flash_write(self):
        self.globals_for_sync();self.fail_at=self.programs+1
        self.packet_call("RemoteHistory_Handle",self.event_wire())
        self.call("RemoteHistory_Tick")
        self.assertEqual(self.queued,[]);self.assertEqual(len(self.records()),0)
    @unittest.skipUnless(REMOTE,"remote endpoint validation")
    def test_remote_rejects_payload_corruption_before_persisting(self):
        self.globals_for_sync()
        wire=bytearray(self.event_wire());wire[27]=2
        self.packet_call("RemoteHistory_Handle",bytes(wire));self.call("RemoteHistory_Tick")
        self.assertEqual(self.queued,[]);self.assertEqual(len(self.records()),0)
    @unittest.skipUnless(REMOTE,"remote endpoint validation")
    def test_remote_rejects_unpaired_serial_without_ack(self):
        self.globals_for_sync()
        self.packet_call("RemoteHistory_Handle",self.event_wire(serial=123))
        self.call("RemoteHistory_Tick")
        self.assertEqual(self.queued,[]);self.assertEqual(len(self.records()),0)
    @unittest.skipUnless(REMOTE,"local monitor events")
    def test_local_link_fault_does_not_claim_motor_off_or_send_slave_ack(self):
        self.globals_for_sync()
        self.call("RemoteHistory_Link",0,0);self.call("RemoteHistory_Tick")
        records=self.records();self.assertEqual(len(records),1)
        event=struct.unpack("<IIIH6B",records[0][:20])
        self.assertEqual(event[4],5);self.assertEqual(event[5],8)
        self.assertEqual(event[6],event[7]);self.assertTrue(event[-1]&128)
        self.assertEqual(self.queued,[])
    @unittest.skipIf(REMOTE,"slave ordered push and retry spacing")
    def test_slave_push_is_fifo_and_retry_is_at_least_ten_seconds(self):
        self.globals_for_sync()
        self.call("Motor_SetPower",1,1);self.call("Motor_SetPower",2,2)
        self.tick_ms(9999);self.call("MotorHistory_Tick");self.assertEqual(self.queued,[])
        self.tick_ms(10000);self.call("MotorHistory_Tick")
        first=self.queued[-1];self.assertEqual(int.from_bytes(first[13:17],"little"),1)
        self.cpu.mem_write(0x20009000,bytes(self.make_packet(first)))
        self.call("MotorHistory_TxFinished",0x20009000)
        self.tick_ms(19999);self.call("MotorHistory_Tick");self.assertEqual(len(self.queued),1)
        self.tick_ms(20000);self.call("MotorHistory_Tick")
        self.assertEqual(self.queued[-1][9:29],first[9:29])
        self.assertEqual(len(self.queued),2)
    @unittest.skipIf(REMOTE,"slave persistent ACK")
    def test_slave_requires_matching_id_hash_then_stops_retrying_after_reset(self):
        self.globals_for_sync();self.call("Motor_SetPower",1,1)
        self.tick_ms(10000);self.call("MotorHistory_Tick")
        event=self.queued[-1][9:29]
        for id,crc in [(99,zlib.crc32(event)),(1,0)]:
            ack=struct.pack("<HBBBBHB",0xff55,19,1,1,2,42,9)+struct.pack("<III",0xabcdef01,id,crc)+b"\x00"
            self.packet_call("MotorHistory_HandleAck",ack)
            self.assertNotEqual(self.call("HJ_Pending"),0)
        ack=struct.pack("<HBBBBHB",0xff55,19,1,1,2,42,9)+struct.pack("<III",0xabcdef01,1,zlib.crc32(event))+b"\x00"
        self.packet_call("MotorHistory_HandleAck",ack)
        self.call("MotorHistory_Init")
        self.assertEqual(self.call("HJ_Pending"),0)
        self.tick_ms(30000);self.call("MotorHistory_Tick");self.assertEqual(len(self.queued),1)
    @unittest.skipIf(REMOTE,"IO hook only, no physical input activation")
    def test_io_hook_logs_source_and_state_without_enabling_inputs(self):
        self.globals_for_sync()
        self.call("Motor_SetPower",1,2)
        event=struct.unpack("<IIIH6B",self.records()[0][:20])
        self.assertEqual(event[4:9],(1,2,2,1,1))
if __name__=="__main__":unittest.main(verbosity=2)
