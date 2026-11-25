"""ARM tests for numeric timer focus, cancel behavior and uncluttered countdown."""
import re
import struct
import subprocess
import unittest
import test_visibility as h
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                              UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_PC, UC_ARM_REG_LR)


class TimerUiTests(h.FirmwareHarness):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        dwarf = subprocess.check_output([str(h.FROMELF), "--text", "-g",
                                         str(h.BUILD / "USB_Application.axf")], text=True)
        cls.offset = {}
        for name in ["currentTime", "scheduleTime", "pwrState"]:
            cls.offset[name] = max(map(int, re.findall(
                r"DW_AT_name " + name + r"\s+[^\n]*\n[^\n]*DW_AT_data_member_location[^\n]*DW_OP_plus_uconst (\d+)", dwarf)))

    def setUp(self):
        super().setUp()
        self.add(0)
        self.select(0)
        self.module = self.state_of(0)
        self.texts, self.backgrounds, self.rectangles = {}, {}, {}
        self.visible = {8: True}
        self.sent = []
        self.creates = 0
        self.cpu.mem_write(h.symbol("deviceControlScreen")[0] + 4,
                           struct.pack("<II", h.symbol("screen_dcs.o:init")[0] | 1,
                                       h.symbol("screen_dcs.o:render")[0] | 1))
        names = ["Main_SetBuzzerMode", "Main_SetScheduleTimeToCurrentModule", "UG_TextboxSetText",
                 "UG_TextboxSetBackColor", "UG_TextboxSetForeColor", "UG_TextboxCheckVisible",
                 "UG_TextboxShow", "UG_TextboxHide", "UG_Update", "ScrnCommon_UpdateBattery",
                 "screen_dcs.o:createText", "screen_dcs.o:init", "screen_dcs.o:render", "UG_WindowCreate", "UG_WindowHide",
                 "UG_WindowDelete", "UG_WindowSetStyle", "UG_WindowSetTitleHeight",
                 "UG_WindowSetBackColor", "UG_WindowSetXStart", "UG_WindowSetYStart",
                 "UG_WindowSetXEnd", "UG_WindowSetYEnd", "UG_WindowShow", "ScrnCommon_StatusBarCreate",
                 "ScrnCommon_StatusBarUpdate", "Util_ConvertTimestampToRTC", "Power_WakeForError"]
        hooks = {h.symbol(name)[0] & ~1: name for name in names}

        def intercept(cpu, address, size, data):
            if address not in hooks:
                return
            name = hooks[address]
            args = [cpu.reg_read(reg) for reg in (UC_ARM_REG_R0, UC_ARM_REG_R1,
                                                  UC_ARM_REG_R2, UC_ARM_REG_R3)]
            result = 0
            if name == "Main_SetScheduleTimeToCurrentModule": self.sent.append(args[0])
            elif name == "UG_TextboxSetText": self.texts[args[1]] = self.string(args[2])
            elif name == "UG_TextboxSetBackColor": self.backgrounds[args[1]] = args[2]
            elif name == "UG_TextboxCheckVisible": result = int(self.visible.get(args[1], True))
            elif name in ["UG_TextboxShow", "UG_TextboxHide"]:
                self.visible[args[1]] = name == "UG_TextboxShow"
            elif name == "screen_dcs.o:createText":
                extra = struct.unpack("<5I", bytes(cpu.mem_read(cpu.reg_read(UC_ARM_REG_SP), 20)))
                self.rectangles[args[1]] = (args[2], args[3], extra[0], extra[1])
            elif name == "UG_WindowCreate": self.creates += 1
            elif name == "Util_ConvertTimestampToRTC": cpu.mem_write(args[1], bytes([14, 35, 0]))
            cpu.reg_write(UC_ARM_REG_R0, result)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))

        self.cpu.hook_add(UC_HOOK_CODE, intercept)

    def string(self, address):
        return bytes(self.cpu.mem_read(address, 96)).split(b"\0", 1)[0].decode()

    def minutes(self, value=None):
        addr = h.symbol("scheduleCustomMinutes")[0]
        if value is not None: self.cpu.mem_write(addr, struct.pack("<H", value))
        return int.from_bytes(self.cpu.mem_read(addr, 2), "little")

    def editor(self, minutes=0, digit=0):
        self.put(h.symbol("inScheduleSelection")[0], 1)
        self.put(h.symbol("adjustingScheduleValue")[0], 1)
        self.put(h.symbol("indexScheduleValue")[0], digit)
        self.minutes(minutes)

    def test_next_moves_focus_across_four_digits_skipping_colon(self):
        self.editor(12 * 60 + 34)
        for expected in (1, 2, 3, 0):
            self.call("screen_dcs.o:centerClicked")
            self.assertEqual(self.get(h.symbol("indexScheduleValue")[0]), expected)
            self.assertEqual([self.texts[32 + i] for i in range(4)], list("1234"))
            selected = [i for i in range(4) if self.backgrounds[32 + i] != 0x2104]
            self.assertEqual(selected, [expected])

    def test_selected_digit_is_the_only_changed_digit(self):
        for position, expected in enumerate([22 * 60 + 34, 13 * 60 + 34, 12 * 60 + 44, 12 * 60 + 35]):
            self.editor(12 * 60 + 34, position)
            self.call("screen_dcs.o:leftClicked")
            self.assertEqual(self.minutes(), expected)

    def test_minutes_tens_wrap_at_five_not_nine(self):
        self.editor(50, 2)
        self.call("screen_dcs.o:leftClicked")
        self.assertEqual(self.minutes(), 0)
        self.call("screen_dcs.o:rightClicked")
        self.assertEqual(self.minutes(), 50)

    def test_hour_tens_wrap_without_modifying_minutes(self):
        self.editor(90 * 60 + 59, 0)
        self.call("screen_dcs.o:leftClicked")
        self.assertEqual(self.minutes(), 59)

    def test_zero_duration_is_rejected_without_transmission(self):
        self.editor()
        self.call("screen_dcs.o:centerPressed")
        self.assertFalse(self.sent)
        self.assertEqual(self.texts[39], "Minimum: 1 minute")
        self.assertTrue(self.call("ScrnDcs_IsScheduleEditing"))

    def test_custom_apply_uses_numeric_state_not_string_cursor(self):
        self.editor(61)
        self.call("screen_dcs.o:centerPressed")
        self.assertEqual(self.sent, [3660])
        self.assertFalse(self.call("ScrnDcs_IsScheduleEditing"))

    def test_hold_right_cancels_custom_without_sending(self):
        self.editor(61)
        self.call("screen_dcs.o:rightPressed")
        self.assertFalse(self.sent)
        self.assertFalse(self.call("ScrnDcs_IsScheduleEditing"))

    def test_hold_center_in_picker_is_back_not_apply(self):
        self.editor()
        self.put(h.symbol("adjustingScheduleValue")[0], 0)
        self.put(h.symbol("indexScheduleSelection")[0], 2)
        self.call("screen_dcs.o:centerPressed")
        self.assertFalse(self.sent)

    def test_digit_and_separator_boxes_do_not_overlap(self):
        self.editor()
        self.call("screen_dcs.o:createTimerScreen")
        for left, right in [(32, 33), (33, 36), (36, 34), (34, 35)]:
            self.assertLess(self.rectangles[left][2], self.rectangles[right][0])
        self.assertLess(self.rectangles[35][3], self.rectangles[38][1])
        self.assertLess(self.rectangles[40][3], self.rectangles[2][1])

    def countdown(self, deadline, now=1000, power=1, offline=False):
        self.cpu.mem_write(self.module + self.offset["scheduleTime"], struct.pack("<I", deadline))
        self.cpu.mem_write(self.system + self.offset["currentTime"], struct.pack("<I", now))
        self.put(self.module + self.offset["pwrState"], power)
        self.put(self.dev(0, h.DEV_CONNECTION_ERROR), int(offline))
        before = bytes(self.cpu.mem_read(self.module, h.MODULE_STRIDE))
        self.call("screen_dcs.o:countdownUpdate", self.device, self.module)
        self.assertEqual(bytes(self.cpu.mem_read(self.module, h.MODULE_STRIDE)), before)

    def test_active_countdown_separates_time_left_from_deadline(self):
        self.countdown(1060)
        self.assertEqual(self.texts[8], "00:01:00")
        self.assertEqual(self.texts[12], "At 14:35")
        self.assertEqual(self.texts[11], "Auto stop")
        self.assertTrue(self.visible[8])

    def test_pending_and_failed_commands_are_not_displayed_as_not_set(self):
        self.put(self.cfg(0, h.CFG_NODE), 1)
        self.put(h.symbol("scheduleRequest")[0] + 15, 1)
        for state, label in [(1, "Sending"), (3, "Rejected"), (4, "No reply"), (5, "Queue err")]:
            self.put(h.symbol("scheduleRequest")[0] + 19, state)
            self.countdown(0)
            self.assertEqual(self.texts[12], label)
            self.assertFalse(self.visible[8])

    def test_inactive_timer_hides_large_countdown(self):
        self.countdown(0)
        self.assertEqual(self.texts[12], "Not set")
        self.assertFalse(self.visible[8])

    def test_old_deadline_is_not_shown_as_running_after_motor_off(self):
        self.countdown(900, power=2)
        self.assertFalse(self.visible[8])

    def test_expired_but_not_yet_confirmed_off_clamps_to_zero(self):
        self.countdown(900)
        self.assertEqual(self.texts[8], "00:00:00")

    def test_offline_timer_is_clearly_marked_as_last_known(self):
        self.countdown(1060, offline=True)
        self.assertEqual(self.texts[11], "Last timer")
        self.assertEqual(self.texts[12], "Offline")

    def test_countdown_format_is_fixed_width_and_bounded(self):
        for seconds, expected in [(0, "00:00:00"), (59, "00:00:59"), (60, "00:01:00"),
                                  (3599, "00:59:59"), (3600, "01:00:00"),
                                  (359999, "99:59:59"), (0xFFFFFFFF, "99:59:59")]:
            # Exercise the public rendering path: the compiler may inline the formatter.
            self.countdown(seconds if seconds else 1, now=0 if seconds else 1)
            self.assertEqual(self.texts[8], expected)

    def test_connection_warning_does_not_discard_timer_editor(self):
        self.editor(61, 2)
        self.add(1)
        self.call("ScrnDcs_ShowConnectionError", 1)
        self.assertEqual(self.minutes(), 61)
        self.assertEqual(self.get(self.system + 60), 0)
        self.assertEqual(self.creates, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
