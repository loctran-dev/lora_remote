"""Remote timer UI/wire diagnostic using built ARM code, no board access."""
import re
import struct
import subprocess
import unittest
import test_visibility as harness
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_PC, UC_ARM_REG_LR


class RemoteScheduleChecks(harness.FirmwareHarness):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        dwarf = subprocess.check_output([str(harness.FROMELF), "--text", "-g",
                                         str(harness.BUILD / "USB_Application.axf")], text=True)
        cls.time_offset = max(map(int, re.findall(
            r"DW_AT_name currentTime\s+[^\n]*\n[^\n]*DW_AT_data_member_location[^\n]*DW_OP_plus_uconst (\d+)", dwarf)))

    def setUp(self):
        super().setUp()
        self.add(0)
        self.select(0)
        self.put(self.system, 1)
        self.put(self.cfg(0, harness.CFG_NODE), 1)
        self.cpu.mem_write(self.system + self.time_offset, struct.pack("<I", 1000))
        self.cpu.mem_write(0x20008000, b"00h01\0")
        self.cpu.mem_write(harness.symbol("deviceControlScreen")[0] + 4,
                           struct.pack("<II", harness.symbol("screen_dcs.o:init")[0] | 1,
                                       harness.symbol("screen_dcs.o:render")[0] | 1))
        self.packets = []
        names = ["osMessageQueuePut", "Main_SetBuzzerMode", "UG_WindowHide",
                 "UG_WindowDelete", "screen_dcs.o:init", "screen_dcs.o:render", "screen_dcs.o:timerRender"]
        hooks = {harness.symbol(name)[0] & ~1: name for name in names}

        def intercept(cpu, address, size, data):
            if address not in hooks:
                return
            name = hooks[address]
            if name == "osMessageQueuePut":
                self.packets.append(bytes(cpu.mem_read(cpu.reg_read(UC_ARM_REG_R1), 274)))
            cpu.reg_write(UC_ARM_REG_R0, 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))

        self.cpu.hook_add(UC_HOOK_CODE, intercept)

    def mode(self, preset, editing=False):
        self.put(harness.symbol("inScheduleSelection")[0], 1)
        self.put(harness.symbol("indexScheduleSelection")[0], preset)
        self.put(harness.symbol("adjustingScheduleValue")[0], int(editing))

    def payload(self):
        packet = self.packets[-1]
        count = int.from_bytes(packet[10:14], "little")
        wire = packet[18:18 + count]
        self.assertEqual(count, 19)
        self.assertEqual(wire[3:6], bytes([1, 1, 1]))  # Net 1, Node 1, REQUEST.
        self.assertEqual(wire[8], 3)  # SET_SCHEDULE.
        self.assertEqual(wire[-1], -sum(wire[:wire[2]]) & 255)
        return struct.unpack("<BII", wire[9:18])

    def test_custom_00h01_transmits_now_plus_60_seconds(self):
        self.mode(0, editing=True)
        self.cpu.mem_write(harness.symbol("scheduleCustomMinutes")[0], struct.pack("<H", 1))
        self.call("screen_dcs.o:centerPressed")
        self.assertEqual(self.payload(), (0, 1000, 1060))

    def test_30_minute_preset_transmits_now_plus_1800(self):
        self.mode(2)
        self.call("screen_dcs.o:centerClicked")
        self.assertEqual(self.payload(), (0, 1000, 2800))

    def test_off_preset_transmits_zero_deadline(self):
        self.mode(1)
        self.call("screen_dcs.o:centerClicked")
        self.assertEqual(self.payload(), (0, 1000, 0))

    def test_holding_center_on_preset_goes_back_without_sending(self):
        self.mode(1)
        self.call("screen_dcs.o:centerPressed")
        self.assertFalse(self.packets)

    def test_deleted_device_cannot_send_timer(self):
        self.put(self.cfg(0, harness.CFG_DELETE), 1)
        self.call("Main_SetScheduleTimeToCurrentModule", 60)
        self.assertFalse(self.packets)

    def test_hidden_device_cannot_send_timer(self):
        self.put(self.cfg(0, harness.CFG_HIDE), 1)
        self.call("Main_SetScheduleTimeToCurrentModule", 60)
        self.assertFalse(self.packets)


if __name__ == "__main__":
    unittest.main(verbosity=2)
