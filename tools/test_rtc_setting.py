"""ARM clock-setting checks; RTC hardware and backup registers mocked."""
import datetime
import struct
import unittest
import test_visibility as h
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_PC,UC_ARM_REG_LR

class RtcSettingTests(h.FirmwareHarness):
    def setUp(self):
        super().setUp()
        self.cpu.mem_map(0x40000000,0x30000)
        self.time=bytes([0,0,0]);self.date=bytes([6,1,1,0])
        self.backup={};self.writes=[];self.failure=None;self.txs=[]
        names=["HAL_RTC_SetTime","HAL_RTC_SetDate","HAL_RTC_GetTime","HAL_RTC_GetDate",
               "HAL_RTCEx_BKUPRead","HAL_RTCEx_BKUPWrite","HAL_PWR_EnableBkUpAccess",
               "Power_ReadRtcSecond","Power_UserActivity","osMessageQueuePut"]
        hooks={h.symbol(n)[0]&~1:n for n in names}
        def intercept(cpu,address,size,data):
            if address not in hooks:return
            name=hooks[address];a=[cpu.reg_read(r) for r in [UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2]];result=0
            if name==self.failure:result=1
            elif name=="HAL_RTC_SetTime":
                self.time=bytes(cpu.mem_read(a[1],3));self.writes.append("time")
            elif name=="HAL_RTC_SetDate":
                self.date=bytes(cpu.mem_read(a[1],4));self.writes.append("date")
            elif name=="HAL_RTC_GetTime":cpu.mem_write(a[1],self.time)
            elif name=="HAL_RTC_GetDate":cpu.mem_write(a[1],self.date)
            elif name=="HAL_RTCEx_BKUPWrite":self.backup[a[1]]=a[2]
            elif name=="HAL_RTCEx_BKUPRead":result=self.backup.get(a[1],0)
            elif name=="Power_ReadRtcSecond":result=self.time[0]*3600+self.time[1]*60+self.time[2]
            elif name=="osMessageQueuePut":self.txs.append(a[1])
            cpu.reg_write(UC_ARM_REG_R0,result);cpu.reg_write(UC_ARM_REG_PC,cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE,intercept)
        self.stamp=int(datetime.datetime(2026,11,18,23,50,1,tzinfo=datetime.timezone.utc).timestamp())
    def test_sets_complete_calendar_without_motor_or_radio_commands(self):
        self.assertEqual(self.call("Rtc_Set",self.stamp),0)
        self.assertEqual(self.time[:3],bytes([23,50,1]))
        self.assertEqual(self.date[1:],bytes([11,18,26]))
        self.assertEqual(self.txs,[])
        self.assertEqual(self.call("Rtc_Read"),self.stamp)
        self.assertEqual(self.call("Link_IsBusy"),1)  # Sync deferred to system task.
    def test_calendar_survives_hal_handle_reset_using_backup_date(self):
        self.call("Rtc_Set",self.stamp)
        saved=dict(self.backup)
        self.date=bytes([6,1,1,0])
        self.call("Rtc_RestoreDate")
        self.assertEqual(self.date[1:],bytes([11,18,26]))
        self.assertEqual(self.backup,saved)
        self.assertEqual(self.txs,[])
    def test_does_not_write_reserved_bootloader_backup_register(self):
        self.call("Rtc_Set",self.stamp)
        self.assertTrue(set(self.backup).issubset({1,2,3,4}))  # HAL BKP DR1..DR4 IDs.
    def test_restore_accounts_for_elapsed_days_in_hardware_counter(self):
        self.call("Rtc_Set",self.stamp)
        self.date=bytes([6,1,1,0])
        counter=2*86400+1
        self.cpu.mem_write(0x40002818,struct.pack("<II",counter>>16,counter&0xffff))
        self.call("Rtc_RestoreDate")
        self.assertEqual(self.date[1:],bytes([11,20,26]))
    def test_rejects_dates_outside_supported_calendar(self):
        for stamp in [0,946684799,4102444800,0xffffffff]:
            self.assertEqual(self.call("Rtc_Set",stamp),1)
        self.assertEqual(self.writes,[])
    def test_rejects_while_timer_command_is_pending(self):
        self.put(h.symbol("scheduleRequest")[0]+19,1)
        self.assertEqual(self.call("Rtc_Set",self.stamp),2)
        self.assertEqual(self.writes,[])
    def test_rejects_active_timer_on_any_supported_module(self):
        self.add(0)
        # Module 5 of device 0 in pool slot 39; scheduleTime offset 24, power state offset 8.
        self.put(self.mot(39, h.MOT_DEVICE), 0)
        self.put(self.mot(39, h.MOT_MODULE), 5)
        base=self.state_of(39)
        self.put(base+8,1)
        self.cpu.mem_write(base+24,struct.pack("<I",1000))
        self.assertEqual(self.call("Rtc_Set",self.stamp),2)
        self.assertEqual(self.writes,[])
    def test_motor_on_without_timer_is_not_changed_or_commanded(self):
        self.add(0)
        self.put(self.state_of(0)+8,1)
        before=bytes(self.cpu.mem_read(self.state_of(0),h.MODULE_STRIDE*h.MOTOR_SLOTS))
        self.assertEqual(self.call("Rtc_Set",self.stamp),0)
        self.assertEqual(bytes(self.cpu.mem_read(self.state_of(0),h.MODULE_STRIDE*h.MOTOR_SLOTS)),before)
        self.assertEqual(self.txs,[])
    def test_midnight_calendar_update_is_saved_for_next_boot(self):
        self.call("Rtc_Set",self.stamp)
        self.time=bytes([0,0,1]);self.date=bytes([4,11,19,26])
        self.call("Rtc_Read")
        self.date=bytes([6,1,1,0])
        self.call("Rtc_RestoreDate")
        self.assertEqual(self.date[1:],bytes([11,19,26]))
    def test_reports_hal_failure(self):
        self.failure="HAL_RTC_SetTime"
        self.assertEqual(self.call("Rtc_Set",self.stamp),3)
        self.assertEqual(self.writes,[])
    def test_clock_jump_does_not_fake_elapsed_link_monitor_time(self):
        self.call("Link_Init",0);self.call("Link_ClockStep",5)
        self.assertEqual(self.call("Link_Now"),5)
        self.call("Rtc_Set",self.stamp)
        self.call("Link_ClockStep",23*3600+50*60+2)
        self.assertEqual(self.call("Link_Now"),6)
    def test_parser_is_strict_and_does_not_confuse_timer_query(self):
        for text,valid in [(f"TIME {self.stamp}\n",1),("TIME 4294967295\n",1),
                           ("TIME 4294967296\n",0),("TIME -1\n",0),("TIME 123abc\n",0),
                           ("TIME \n",0),("TIME 123\nTIME 4\n",0),("TIMER\n",0)]:
            payload=text.encode();self.cpu.mem_write(0x20008000,payload)
            self.assertEqual(self.call("Rtc_Parse",0x20008000,len(payload),0x20008100),valid,text)

if __name__=="__main__":unittest.main(verbosity=2)
