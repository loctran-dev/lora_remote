"""Built ARM Device-settings navigation, values and non-destructive editor return."""
import struct
import unittest
import test_visibility as harness
from test_general_menu import MenuHarness


class DeviceMenuTests(MenuHarness):
    def setUp(self):
        super().setUp()
        self.mocks(["ScrnCommon_SettingsMenuCreateStacked", "ScrnCommon_SettingsMenuBegin",
                    "ScrnCommon_SettingsMenuPage", "ScrnCommon_SettingsMenuRow",
                    "ScrnCommon_SettingsMenuUpdate", "Main_SetBuzzerMode",
                    "UG_WindowHide", "UG_WindowDelete", "screen_edss.o:init", "screen_edss.o:render"])
        self.add(0)
        self.select(0)
        self.call("Link_Init", 0)
        # Mirror the numeric-locale initialization normally done by C startup.
        locale = self.call("__rt_locale")
        numeric = self.call("_get_lc_numeric", 0, 0)
        self.cpu.mem_write(locale + 12, struct.pack("<I", numeric))
        self.call("Link_SetAutoStatus", 0, 2)
        self.module = self.device + 36
        self.cpu.mem_write(self.module + 40, b"ABCDEFGHIJKLMNOPQRS\0")
        self.cpu.mem_write(self.module + 60, struct.pack("<fI", 12.34, 30))
        self.put(self.module + 68, 1)
        for screen, init, render in [("editDeviceSettingScreen", "screen_edss.o:init", "screen_edss.o:render"),
                                     ("deviceSettingScreen", "screen_dss.o:init", "screen_dss.o:render")]:
            self.cpu.mem_write(harness.symbol(screen)[0] + 4,
                               struct.pack("<II", harness.symbol(init)[0] | 1,
                                           harness.symbol(render)[0] | 1))
        self.call("ScrnDss_ResetSelection")
        self.call("screen_dss.o:init")

    def render(self):
        self.calls.clear()
        self.call("screen_dss.o:render")
        return [call for call in self.calls if call[0] == "ScrnCommon_SettingsMenuRow"]

    def advance(self, count):
        for _ in range(count):
            self.call("screen_dss.o:rightClicked")

    def test_first_page_values_and_selected_row(self):
        rows = self.render()
        self.assertEqual([row[3][0] for row in rows],
                         ["Low-current alarm", "Alarm delay", "Lock", "Hide"])
        self.assertEqual([row[3][1] for row in rows], ["12.34 A", "30 s", "Yes", "No"])
        self.assertEqual([row[1][3] for row in rows], [1, 0, 0, 0])

    def test_second_page_and_unused_fourth_row(self):
        self.advance(4)
        rows = self.render()
        self.assertEqual([row[3][0] for row in rows],
                         ["Delete device", "Device name", "Auto status interval", ""])
        self.assertEqual([row[3][1] for row in rows], ["No", "ABCDEFGHIJKLMNOPQRS", "2 s", ""])
        self.assertIn((4, 7), [call[1][:2] for call in self.calls if call[0] == "ScrnCommon_SettingsMenuPage"])

    def test_all_seven_editors_keep_selection_on_actual_cancel_path(self):
        storage = bytes(self.cpu.mem_read(self.storage, 885))
        device = bytes(self.cpu.mem_read(self.device, 468))
        for position in range(7):
            self.call("screen_dss.o:centerClicked")
            self.assertEqual(self.get(harness.symbol("indexOfSettingMenu")[0]), position)
            self.call("screen_edss.o:rightPressed")
            rows = self.render()
            self.assertTrue(rows[position % 4][1][3])
            self.advance(1)
        self.assertEqual(bytes(self.cpu.mem_read(self.storage, 885)), storage)
        self.assertEqual(bytes(self.cpu.mem_read(self.device, 468)), device)

    def test_bounds_and_new_control_entry_reset(self):
        self.advance(20)
        self.assertEqual([row[1][3] for row in self.render()], [0, 0, 1, 0])
        self.call("ScrnDss_ResetSelection")
        self.call("screen_dss.o:init")
        self.assertTrue(self.render()[0][1][3])
        for _ in range(20):
            self.call("screen_dss.o:leftClicked")
        self.assertTrue(self.render()[0][1][3])

    def test_labels_and_maximum_length_name_fit_the_shared_layout(self):
        for _ in range(2):
            for row in self.render():
                self.assertLessEqual(len(row[3][0]) * 6, 122)
                self.assertLessEqual(len(row[3][1]) * 6, 122)
            self.advance(4)


if __name__ == "__main__":
    unittest.main(verbosity=2)
