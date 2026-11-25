"""Built ARM checks for confirmed, bounded and cancelable timer commands."""
import re
import struct
import subprocess
import unittest
import test_visibility as h
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                              UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_PC, UC_ARM_REG_LR)


class TimerTransactionTests(h.FirmwareHarness):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        dwarf = subprocess.check_output([str(h.FROMELF), "--text", "-g",
                                         str(h.BUILD / "USB_Application.axf")], text=True)
        cls.fields_timer = {}
        for name in ["currentTime", "scheduleTime"]:
            cls.fields_timer[name] = max(map(int, re.findall(
                r"DW_AT_name " + name + r"\s+[^\n]*\n[^\n]*DW_AT_data_member_location[^\n]*DW_OP_plus_uconst (\d+)", dwarf)))

    def setUp(self):
        super().setUp()
        self.add(0)
        self.select(0)
        self.put(self.system, 1)
        self.put(self.cfg(0, h.CFG_NODE), 1)
        self.now(1000)
        self.packets = []
        self.queue_result = 0
        hooks = {h.symbol(n)[0] & ~1: n for n in
                 ["osMessageQueuePut", "Main_SetBuzzerMode", "Link_RecordPacket", "LoRa_getRSSI"]}

        def intercept(cpu, address, size, data):
            if address not in hooks: return
            name = hooks[address]
            if name == "osMessageQueuePut":
                self.packets.append(bytes(cpu.mem_read(cpu.reg_read(UC_ARM_REG_R1), 274)))
            cpu.reg_write(UC_ARM_REG_R0, (self.queue_result if name == "osMessageQueuePut" else 0) & 0xFFFFFFFF)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE, intercept)

    def now(self, seconds):
        self.cpu.mem_write(self.system + self.fields_timer["currentTime"], struct.pack("<I", seconds))

    def tick(self, milliseconds):
        self.cpu.mem_write(h.symbol("uwTick")[0], struct.pack("<I", milliseconds))
        self.call("Schedule_Tick")

    def state(self):
        return self.get(h.symbol("scheduleRequest")[0] + 19)

    def start(self, seconds=60):
        self.call("Main_SetScheduleTimeToCurrentModule", seconds)
        return self.packets[-1]

    def reply(self, result=1, node=1, transaction=None, module=0):
        request = self.packets[0]
        transaction = transaction if transaction is not None else int.from_bytes(request[6:8], "little")
        packet = bytearray(274)
        packet[:9] = struct.pack("<HBBBBHB", 0xFF55, 9, 1, node, 2, transaction, 3)
        packet[10:14] = struct.pack("<I", 12)
        packet[18:27] = packet[:9]
        packet[27:29] = bytes([module, result])
        stack, stop = 0x2000E000, 0x0807FFF0
        self.cpu.mem_write(stack, bytes(packet[16:]))
        for reg, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3),
                              struct.unpack("<4I", packet[:16])):
            self.cpu.reg_write(reg, value)
        self.cpu.reg_write(UC_ARM_REG_SP, stack)
        self.cpu.reg_write(UC_ARM_REG_LR, stop | 1)
        self.cpu.emu_start(h.symbol("handleSetSchedule")[0] | 1, stop, count=20000)
        self.assertEqual(self.cpu.reg_read(UC_ARM_REG_PC), stop)

    def test_retries_keep_identical_transaction_payload_and_checksum(self):
        first = self.start()
        self.tick(2999)
        self.assertEqual(len(self.packets), 1)
        self.tick(3000)
        self.tick(6000)
        self.assertEqual(len(self.packets), 3)
        self.assertEqual([p[18:37] for p in self.packets], [first[18:37]] * 3)
        self.tick(9000)
        self.assertEqual(self.state(), 4)
        self.tick(15000)
        self.assertEqual(len(self.packets), 3)

    def test_matching_success_confirms_deadline_and_stops_retries(self):
        self.start()
        self.reply()
        self.assertEqual(self.state(), 2)
        deadline = int.from_bytes(self.cpu.mem_read(self.state_of(0) + self.fields_timer["scheduleTime"], 4), "little")
        self.assertEqual(deadline, 1060)
        self.tick(10000)
        self.assertEqual(len(self.packets), 1)

    def test_rejection_is_not_success_and_does_not_retry(self):
        self.start()
        self.reply(result=0)
        self.assertEqual(self.state(), 3)
        self.tick(10000)
        self.assertEqual(len(self.packets), 1)

    def test_wrong_node_transaction_and_module_cannot_confirm(self):
        self.start()
        for args in [dict(node=2), dict(transaction=999), dict(module=1)]:
            self.reply(**args)
            self.assertEqual(self.state(), 1)

    def test_old_ack_cannot_confirm_a_replacement(self):
        self.start()
        self.start(120)
        self.reply()
        self.assertEqual(self.state(), 1)

    def test_queue_failure_is_visible(self):
        self.queue_result = -3
        self.start()
        self.assertEqual(self.state(), 5)

    def test_manual_off_invalidates_queued_old_timer_and_retry(self):
        first = self.start()
        self.call("Main_SetMotorPower", 2)
        self.assertEqual(self.state(), 0)
        self.cpu.mem_write(0x20009000, first)
        self.assertEqual(self.call("Schedule_ShouldTransmit", 0x20009000), 0)
        self.tick(10000)
        self.assertEqual(len(self.packets), 2)  # Original timer + explicit OFF only.

    def test_other_node_cancel_does_not_cancel_current_request(self):
        self.start()
        self.call("Schedule_CancelForNode", 2)
        self.assertEqual(self.state(), 1)

    def test_expired_deadline_is_not_retransmitted(self):
        self.start(1)
        self.now(1002)
        self.tick(3000)
        self.assertEqual(self.state(), 4)
        self.assertEqual(len(self.packets), 1)

    def test_late_matching_ack_can_recover_timeout_without_resending(self):
        self.start()
        for milliseconds in [3000, 6000, 9000]: self.tick(milliseconds)
        self.assertEqual(self.state(), 4)
        self.reply()
        self.assertEqual(self.state(), 2)
        self.assertEqual(len(self.packets), 3)

    def test_ack_after_explicit_cancel_cannot_restore_old_timer(self):
        self.start()
        self.call("Schedule_CancelForNode", 1)
        self.reply()
        self.assertEqual(self.state(), 0)
        deadline = int.from_bytes(self.cpu.mem_read(self.state_of(0) + self.fields_timer["scheduleTime"], 4), "little")
        self.assertEqual(deadline, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
