"""Run the built ARM General-settings navigation and layout without board I/O."""
import struct
import unittest
import test_visibility as harness
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                              UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR,
                              UC_ARM_REG_PC)


class MenuHarness(harness.FirmwareHarness):
    def mocks(self, names):
        self.calls = []
        addresses = {harness.symbol(name)[0] & ~1: name for name in names}

        def intercept(cpu, address, size, data):
            if address not in addresses:
                return
            name = addresses[address]
            args = tuple(cpu.reg_read(reg) for reg in
                         (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3))
            extra = struct.unpack("<4I", bytes(cpu.mem_read(cpu.reg_read(UC_ARM_REG_SP), 16)))
            strings = ()
            if name == "Screen_SettingsMenuRow":
                strings = (self.cstring(args[1]), self.cstring(args[2]))
            elif name == "UG_TextboxSetText":
                strings = (self.cstring(args[2]),)
            self.calls.append((name, args, extra, strings))
            cpu.reg_write(UC_ARM_REG_R0, 1 if name == "UG_TextboxCheckVisible" else 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))

        self.cpu.hook_add(UC_HOOK_CODE, intercept)

    def cstring(self, address):
        if not address:
            return ""
        return bytes(self.cpu.mem_read(address, 96)).split(b"\0", 1)[0].decode()


class GeneralMenuTests(MenuHarness):
    def setUp(self):
        super().setUp()
        self.mocks(["Screen_SettingsMenuCreateStacked", "Screen_SettingsMenuBegin",
                    "Screen_SettingsMenuPage", "Screen_SettingsMenuRow",
                    "Screen_SettingsMenuUpdate", "Main_SetBuzzerMode",
                    "UG_WindowHide", "UG_WindowDelete", "screen_egss.o:init", "screen_egss.o:render", "screen_dts.o:init", "screen_dts.o:render"])
        self.call("Link_Init", 0)
        # Initialized-RW screen vtable is copied by scatter-load on real boot.
        self.cpu.mem_write(harness.symbol("editGeneralSettingScreen")[0] + 4,
                           struct.pack("<II", harness.symbol("screen_egss.o:init")[0] | 1,
                                       harness.symbol("screen_egss.o:render")[0] | 1))
        self.cpu.mem_write(harness.symbol("dateTimeScreen")[0] + 4,
                           struct.pack("<II",harness.symbol("screen_dts.o:init")[0]|1,harness.symbol("screen_dts.o:render")[0]|1))
        self.call("Link_SetIdleSeconds", 45)
        self.call("Link_SetTimeoutSeconds", 180)
        self.cpu.mem_write(self.system + self.fields["tUpdate"], struct.pack("<I", 5))
        self.cpu.mem_write(self.system + self.fields["tSleepUpdate"], struct.pack("<I", 300))
        self.call("Screen_GeneralSettingsResetSelection")
        self.call("screen_gss.o:init")

    def render(self):
        self.calls.clear()
        self.call("screen_gss.o:render")
        return [call for call in self.calls if call[0] == "Screen_SettingsMenuRow"]

    def advance(self, count):
        for _ in range(count):
            self.call("screen_gss.o:rightClicked")

    def test_sleep_fields_are_grouped_on_first_page(self):
        rows = self.render()
        self.assertEqual([row[3][0] for row in rows],
                         ["Enable sleep mode", "Sleep after idle", "Sleep check interval", "Link timeout"])
        self.assertEqual([row[3][1] for row in rows][1:], ["45 s", "300 s", "180 s"])

    def test_radio_fields_are_grouped_on_second_page(self):
        self.advance(4)
        rows = self.render()
        self.assertEqual([row[3][0] for row in rows],
                         ["Link scan interval", "LoRa network ID", "LoRa frequency", "LoRa spread factor"])
        self.assertEqual(sum(bool(row[1][3]) for row in rows), 1)
        self.assertIn((4, 9), [call[1][:2] for call in self.calls if call[0] == "Screen_SettingsMenuPage"])

    def test_every_item_opens_its_original_editor_and_keeps_selection(self):
        before = bytes(self.cpu.mem_read(self.storage, 885))
        for position, original_id in enumerate([0, 6, 2, 7, 1, 3, 4, 5]):
            self.call("screen_gss.o:centerClicked")
            self.assertEqual(self.get(harness.symbol("indexOfGeneralSettingMenu")[0]), original_id)
            self.call("screen_gss.o:init")  # Same path as editor save/cancel return.
            rows = self.render()
            self.assertTrue(rows[position % 4][1][3])
            self.advance(1)
        self.assertEqual(bytes(self.cpu.mem_read(self.storage, 885)), before)

    def test_navigation_bounds_and_fresh_menu_entry(self):
        for _ in range(12):
            self.call("screen_gss.o:leftClicked")
        self.assertTrue(self.render()[0][1][3])
        self.advance(12)
        self.assertTrue(self.render()[0][1][3])
        self.call("Screen_GeneralSettingsResetSelection")
        self.call("screen_gss.o:init")
        rows = self.render()
        self.assertEqual(rows[0][3][0], "Enable sleep mode")
        self.assertTrue(rows[0][1][3])

    def test_labels_fit_full_width_without_scrolling(self):
        for _ in range(2):
            for row in self.render():
                self.assertLessEqual(len(row[3][0]) * 6, 122)
            self.advance(4)

    def test_third_page_only_shows_date_time_and_opens_its_editor(self):
        self.advance(8)
        rows=self.render()
        self.assertEqual(rows[0][3][0],"Date & time")
        self.assertTrue(rows[0][1][3])
        self.assertTrue(all(row[1][1:3]==(0,0) for row in rows[1:]))
        self.call("screen_gss.o:centerClicked")
        self.assertEqual(self.get(harness.symbol("indexOfGeneralSettingMenu")[0]),8)
        self.assertEqual(int.from_bytes(self.cpu.mem_read(harness.symbol("currentScreen")[0],4),"little"),
                         harness.symbol("dateTimeScreen")[0])
        self.assertTrue(self.render()[0][1][3])


class SettingsLayoutTests(MenuHarness):
    def setUp(self):
        super().setUp()
        self.mocks(["screens.o:createWindow", "screens.o:createText", "UG_ImageCreate", "UG_ImageSetBMP",
                    "UG_TextboxSetAlignment", "UG_TextboxSetForeColor",
                    "UG_TextboxSetScrollEnabled", "UG_TextboxSetText",
                    "UG_TextboxSetBackColor", "UG_TextboxCheckVisible",
                    "UG_TextboxShow", "UG_TextboxHide", "UG_WindowShow", "UG_FillFrame"])

    def create(self):
        self.cpu.mem_write(0x20008000, b"General setup\0")
        self.call("Screen_SettingsMenuCreateStacked", 0x20008000)
        return {call[1][1]: (*call[1][2:], *call[2][:2])
                for call in self.calls if call[0] == "screens.o:createText"}

    def test_label_and_value_rectangles_never_overlap(self):
        boxes = self.create()
        for row in range(4):
            label, value = boxes[6 + row * 2], boxes[7 + row * 2]
            self.assertEqual((label[0], label[2]), (3, 124))
            self.assertEqual((value[0], value[2]), (3, 124))
            self.assertLess(label[3], value[1])
            self.assertLess(value[3], 144)
            if row < 3:
                self.assertLess(value[3], boxes[8 + row * 2][1])
        self.assertNotIn(14, boxes)  # Four two-line rows, no fifth stale row.
        self.assertLess(boxes[20][2], boxes[21][0])  # Header / page counter.

    def test_page_counter_and_clear_cover_both_text_lines(self):
        self.create()
        self.call("Screen_SettingsMenuBegin", 4)
        self.call("Screen_SettingsMenuPage", 4, 8)
        self.assertIn("2/2", [call[3][0] for call in self.calls if call[0] == "UG_TextboxSetText"])
        clear = [call for call in self.calls if call[0] == "UG_FillFrame"][-1]
        self.assertEqual(clear[1], (0, 42, 127, 139))

    def test_seven_item_device_menu_also_has_two_pages(self):
        self.create()
        for page, text in [(0, "1/2"), (4, "2/2")]:
            self.call("Screen_SettingsMenuPage", page, 7)
            self.assertEqual([call[3][0] for call in self.calls
                              if call[0] == "UG_TextboxSetText"][-1], text)

    def test_values_do_not_scroll_even_when_row_is_selected(self):
        self.create()
        self.cpu.mem_write(0x20008000, b"Sleep after idle\0")
        self.cpu.mem_write(0x20008100, b"9999 s\0")
        # Fourth argument is selected=1; call helper supports three arguments.
        self.cpu.reg_write(UC_ARM_REG_R3, 1)
        self.call("Screen_SettingsMenuRow", 0, 0x20008000, 0x20008100)
        scrolls = [call[1][1:3] for call in self.calls if call[0] == "UG_TextboxSetScrollEnabled"]
        self.assertIn((6, 1), scrolls)
        self.assertIn((7, 0), scrolls)

    def test_unused_device_page_row_is_hidden_then_restored(self):
        self.create()
        self.call("Screen_SettingsMenuBegin", 4)
        self.call("Screen_SettingsMenuRow", 3, 0, 0)
        hidden = [call[1][1] for call in self.calls if call[0] == "UG_TextboxHide"]
        self.assertEqual(hidden, [12, 13])
        self.cpu.mem_write(0x20008000, b"Hide\0")
        self.cpu.mem_write(0x20008100, b"No\0")
        self.call("Screen_SettingsMenuBegin", 0)
        self.call("Screen_SettingsMenuRow", 3, 0x20008000, 0x20008100)
        shown = [call[1][1] for call in self.calls if call[0] == "UG_TextboxShow"]
        self.assertEqual(shown[-2:], [12, 13])


if __name__ == "__main__":
    unittest.main(verbosity=2)
