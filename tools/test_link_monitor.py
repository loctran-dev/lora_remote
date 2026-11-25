"""Execute the actual RTC-based link supervisor in ARM emulation, no board I/O."""
import re
import struct
import unittest
import test_visibility as harness
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_PC, UC_ARM_REG_LR

class LinkTests(harness.FirmwareHarness):
    def setUp(self):
        super().setUp()
        self.add(0)
        self.put(self.device + 1, 1)
        self.put(self.system, 1)
        self.put(self.system + 92, 2)  # SYS_MODE_NORMAL; legacy layout unchanged.
        self.cpu.mem_write(self.system + self.fields["tUpdate"], struct.pack("<I", 1))
        self.queue_ok = True
        self.queued, self.alerts = [], []
        self.history_events=[]
        self.probe = harness.symbol("linkProbe")[0]
        self.states = harness.symbol("linkDevices")[0]
        assert harness.symbol("linkProbe")[1] == 28
        def queue(cpu, address, size, data):
            if self.queue_ok:
                self.queued.append(bytes(cpu.mem_read(cpu.reg_read(UC_ARM_REG_R1), 274)))
            cpu.reg_write(UC_ARM_REG_R0, 0 if self.queue_ok else 0xFFFFFFFD)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        def alert(cpu, address, size, data):
            self.alerts.append(cpu.reg_read(UC_ARM_REG_R0))
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        def history(cpu,address,size,data):
            self.history_events.append((cpu.reg_read(UC_ARM_REG_R0),cpu.reg_read(UC_ARM_REG_R1)))
            cpu.reg_write(UC_ARM_REG_PC,cpu.reg_read(UC_ARM_REG_LR))
        for name, callback in [("osMessageQueuePut", queue), ("ScrnDcs_ShowConnectionError", alert),
                               ("RemoteHistory_Link",history)]:
            address = harness.symbol(name)[0] & ~1
            self.cpu.hook_add(UC_HOOK_CODE, callback, begin=address, end=address)
        self.call("Link_Init", 0)
        self.call("Link_SetTimeoutSeconds", 5)

    def at(self, seconds):
        self.cpu.mem_write(harness.symbol("uwTick")[0], struct.pack("<I", (seconds * 1000) & 0xFFFFFFFF))
        self.call("Link_ClockStep", seconds)
        self.call("Link_Tick")

    def sent(self, success=1):
        self.cpu.mem_write(0x20008800, self.queued[-1])
        self.call("Link_TxFinished", 0x20008800, success)

    def error(self, id=0):
        return self.get(self.device + id * 468 + 32)

    def test_no_requests_before_or_at_timeout(self):
        for second in range(6):
            self.at(second)
        self.assertFalse(self.queued)
        self.assertFalse(self.alerts)

    def test_three_requests_waiting_ten_seconds_each(self):
        self.at(6)
        self.assertEqual(len(self.queued), 1)
        self.sent()
        self.at(15)
        self.assertEqual(len(self.queued), 1)
        self.at(16)
        self.assertEqual(len(self.queued), 2)
        self.sent()
        self.at(25)
        self.assertEqual(len(self.queued), 2)
        self.at(26)
        self.assertEqual(len(self.queued), 3)
        self.sent()
        self.at(35)
        self.assertFalse(self.alerts)
        self.at(36)
        self.assertEqual(self.alerts, [0])
        self.assertEqual(self.error(), 1)
        self.assertEqual(len(self.queued), 3)
        self.at(37)
        self.assertEqual(len(self.queued), 3)  # Failure cooldown, not a tight retry loop.

    def test_status_confirms_and_cancels_pending_request(self):
        self.at(6)
        self.call("Link_RecordPacket", 0, 1)
        self.at(7)
        self.assertEqual(self.call("Link_IsBusy"), 0)
        self.assertEqual(self.error(), 0)
        self.cpu.mem_write(0x20008800, self.queued[-1])
        self.assertEqual(self.call("Link_ShouldTransmit", 0x20008800), 0)

    def test_ack_alone_does_not_cancel_confirmation(self):
        self.at(6)
        self.sent()
        self.call("Link_RecordPacket", 0, 0)
        self.at(16)
        self.assertEqual(len(self.queued), 2)
        self.assertEqual(self.call("Link_IsBusy"), 1)

    def test_healthy_status_traffic_never_proactively_polls(self):
        for second in range(1, 40):
            self.call("Link_ClockStep", second)
            if second % 3 == 0:
                self.call("Link_RecordPacket", 0, 1)
            self.call("Link_Tick")
        self.assertFalse(self.queued)

    def test_sleep_waits_for_rtc_or_radio_scan_request(self):
        self.put(self.system + 92, 1)
        self.at(20)
        self.assertFalse(self.queued)
        self.call("Link_RequestScan")
        self.at(20)
        self.assertEqual(len(self.queued), 1)

    def test_receiving_from_one_device_does_not_mask_other(self):
        self.add(1)
        self.put(self.device + 468 + 1, 2)
        self.call("Link_Init", 0)
        self.call("Link_SetTimeoutSeconds", 5)
        self.call("Link_ClockStep", 6)
        self.call("Link_RecordPacket", 0, 1)
        self.call("Link_Tick")
        self.assertEqual(self.queued[-1][4], 2)

    def test_hidden_and_locked_devices_are_still_monitored(self):
        self.put(self.device + 36 + 68, 1)
        self.put(self.device + 36 + 69, 1)
        self.at(6)
        self.assertEqual(len(self.queued), 1)

    def test_deleted_device_is_not_monitored(self):
        self.put(self.device + 20, 1)
        self.at(100)
        self.assertFalse(self.queued)

    def test_deleted_pending_probe_is_canceled(self):
        self.at(6)
        self.put(self.device + 20, 1)
        self.at(100)
        self.assertFalse(self.alerts)
        self.assertEqual(self.call("Link_IsBusy"), 0)

    def test_queue_full_does_not_count_as_three_transmissions(self):
        self.queue_ok = False
        self.at(6)
        self.at(40)
        self.assertEqual(self.get(self.probe + 23), 0)
        self.assertFalse(self.alerts)
        self.queue_ok = True
        self.at(41)
        self.assertEqual(self.get(self.probe + 23), 1)

    def test_late_status_recovers_confirmed_fault(self):
        for second in (6, 16, 26):
            self.at(second)
            self.sent()
        self.at(36)
        self.assertEqual(self.error(), 1)
        self.call("Link_RecordPacket", 0, 1)
        self.assertEqual(self.error(), 0)
        self.assertEqual(self.history_events,[(0,0),(0,1)])

    def test_rtc_day_rollover(self):
        self.call("Link_Init", 86399)
        self.call("Link_ClockStep", 1)
        self.assertEqual(self.call("Link_Now"), 2)

    def test_multiple_offline_nodes_do_not_starve_later_devices(self):
        for id in (1, 2):
            self.add(id)
            self.put(self.device + id * 468 + 1, id + 1)
        self.call("Link_Init", 0)
        self.call("Link_SetTimeoutSeconds", 5)
        self.at(6)
        for start in (6, 37):
            self.sent()
            self.at(start + 10)
            self.sent()
            self.at(start + 20)
            self.sent()
            self.at(start + 30)
            self.at(start + 31)
        self.assertEqual([self.queued[index][4] for index in (0, 3, 6)], [1, 2, 3])

    def test_legacy_flash_extension_defaults_and_roundtrip(self):
        self.assertEqual(self.call("Link_GetIdleSeconds"), 60)
        self.assertEqual(self.call("Link_GetTimeoutSeconds"), 5)
        self.assertEqual(self.call("Link_GetAutoStatus", 0), 60)
        self.call("Link_SetIdleSeconds", 7)
        self.call("Link_SetTimeoutSeconds", 9)
        self.call("Link_SetAutoStatus", 0, 2)
        self.call("Link_Init", 0)
        self.assertEqual(self.call("Link_GetIdleSeconds"), 7)
        self.assertEqual(self.call("Link_GetTimeoutSeconds"), 9)
        self.assertEqual(self.call("Link_GetAutoStatus", 0), 2)

    def status_reply(self,id=0,transaction=None):
        if transaction is None:transaction=int.from_bytes(self.queued[-1][6:8],"little")
        self.call("Link_RecordPacket",id,1)
        self.call("Link_RecordStatusReply",id,transaction)

    def test_startup_sync_sends_before_timeout_with_current_clock(self):
        self.cpu.mem_write(self.system+36,struct.pack("<I",1795046400))
        self.call("Link_RequestStatusSync")
        self.assertEqual(self.call("Link_IsBusy"),1)
        self.assertEqual(self.queued,[])  # Safe even if requested from USB IRQ.
        self.at(0)
        self.assertEqual(len(self.queued),1)
        raw=self.queued[-1][18:]
        self.assertEqual((raw[4],raw[8]),(1,1))  # Unicast node, GET_STATUS.
        self.assertEqual(struct.unpack_from("<I",raw,10)[0],1795046400)
        self.status_reply();self.at(1)
        self.assertEqual(self.call("Link_IsBusy"),0)
        self.assertEqual(len(self.queued),1)
    def test_unsolicited_status_does_not_cancel_startup_sync(self):
        self.call("Link_RequestStatusSync");self.at(0);self.sent()
        self.status_reply(transaction=0);self.at(1)
        self.assertEqual(self.call("Link_IsBusy"),1)
        self.at(10)
        self.assertEqual(len(self.queued),2)
    def test_startup_visits_hidden_and_locked_nodes_once_then_stops_polling(self):
        self.add(1,True);self.put(self.device+468+1,2)
        self.call("Link_Init",0);self.call("Link_RequestStatusSync");self.at(0)
        self.status_reply(0);self.at(1);self.at(2)
        self.assertEqual([packet[4] for packet in self.queued],[1,2])
        self.status_reply(1);self.at(3)
        self.assertEqual(self.call("Link_IsBusy"),0)
        self.at(4)
        self.assertEqual(len(self.queued),2)
    def test_startup_missing_slave_uses_three_ten_second_attempts(self):
        self.call("Link_RequestStatusSync")
        for second in [0,10,20]:self.at(second);self.sent()
        self.at(29);self.assertFalse(self.alerts)
        self.at(30)
        self.assertEqual(len(self.queued),3)
        self.assertEqual(self.alerts,[0])
        self.assertEqual(self.call("Link_IsBusy"),0)
    def test_unset_remote_clock_is_not_sent_as_a_sync_timestamp(self):
        self.call("Link_RequestStatusSync");self.at(0)
        self.assertEqual(struct.unpack_from("<I",self.queued[-1],28)[0],0)
    def test_requesting_again_invalidates_old_queued_transaction(self):
        self.call("Link_RequestStatusSync");self.at(0)
        old=self.queued[-1]
        self.call("Link_RequestStatusSync");self.at(1)
        self.cpu.mem_write(0x20008800,old)
        self.assertEqual(self.call("Link_ShouldTransmit",0x20008800),0)
        self.assertNotEqual(self.queued[-1][6:8],old[6:8])
    def test_transaction_zero_is_reserved_for_autonomous_status(self):
        self.cpu.mem_write(harness.symbol("currentTransactionID")[0],struct.pack("<H",65535))
        self.assertEqual(self.call("generateTransactionID"),1)

if __name__ == "__main__":
    unittest.main(verbosity=2)
