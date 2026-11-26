"""Built-ARM checks of the device setting editor and pairing screen fixes:
KI-4 (no edit without a motor slot), KI-7 (no flash rewrite when Lock/Hide/Delete
do not change), KI-8 (pairing screen drawn once). No board access."""
import unittest

import test_visibility as h
from test_visibility import CFG_DELETE, MOT_HIDE, MOT_LOCK
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_PC, UC_ARM_REG_LR

LONG_BEEP = 4                       # eBuzzer_Mode_LongBeep
ITEM_THRESHOLD, ITEM_LOCK, ITEM_HIDE, ITEM_DELETE, ITEM_NAME, ITEM_AUTO = 0, 2, 3, 4, 5, 6
CONNECTING, NO_ROOM = 3, 7          # PAIRING_STATE_...
SAVE_PENDING = 62                   # SystemInfo_t offset of the "save configuration" flag


class ScreenFixTests(h.FirmwareHarness):
    def setUp(self):
        super().setUp()
        self.add(0)                                                  # device 0, module 0 in pool slot 0
        self.select(0)
        self.beeps, self.switches = [], []
        self.connect_ok = 1
        names = ["Main_SetBuzzerMode", "ScrnCommon_Switch", "Main_ConnectToDevice", "Main_RestoreRadio",
                 "Main_ScanNewDevice", "CmdHandler_SendCurrentModuleConfig"]
        hooks = {h.symbol(n)[0] & ~1: n for n in names}

        def intercept(cpu, address, size, data):
            name = hooks.get(address)
            if name is None:
                return
            result = 0
            if name == "Main_SetBuzzerMode":
                self.beeps.append(cpu.reg_read(UC_ARM_REG_R0))
            elif name == "ScrnCommon_Switch":
                self.switches.append((cpu.reg_read(UC_ARM_REG_R0), self.get(h.symbol("pairingUI_Step")[0])))
            elif name == "Main_ConnectToDevice":
                result = self.connect_ok
            elif name == "CmdHandler_SendCurrentModuleConfig":
                result = 1
            cpu.reg_write(UC_ARM_REG_R0, result)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE, intercept)

    def screen(self, name):
        return h.symbol(name)[0]

    def edit(self, item, yes=None):
        """Open the editor on item; for Yes/No items choose Yes (True) or No (False)."""
        self.put(h.symbol("indexOfSettingMenu")[0], item)
        if yes is not None:
            self.put(h.symbol("screen_edss.o:indexOfSelection")[0], 0 if yes else 1)

    def saved(self):
        return self.get(self.system + SAVE_PENDING)

    # KI-4 -----------------------------------------------------------------
    def test_motor_item_of_a_motor_without_a_slot_is_refused(self):
        self.select(0, 1)                                            # module 1 has no slot
        for item in (ITEM_THRESHOLD, ITEM_LOCK, ITEM_HIDE, ITEM_NAME):
            self.put(h.symbol("screen_dss.o:menuIndex")[0], item)
            self.call("screen_dss.o:centerClicked")
        self.assertEqual(self.beeps, [LONG_BEEP] * 4)
        self.assertEqual(self.switches, [])                          # editor never opened

    def test_device_items_open_without_a_motor_slot(self):
        self.select(0, 1)
        for item in (ITEM_DELETE, ITEM_AUTO):
            self.put(h.symbol("screen_dss.o:menuIndex")[0], item)
            self.call("screen_dss.o:centerClicked")
        self.assertEqual([s[0] for s in self.switches], [self.screen("editDeviceSettingScreen")] * 2)

    def test_editor_does_not_store_into_the_placeholder(self):
        self.select(0, 1)
        self.edit(ITEM_LOCK, yes=True)
        self.call("screen_edss.o:centerPressed")
        self.edit(ITEM_LOCK, yes=True)
        self.call("screen_edss.o:centerClicked")
        self.assertEqual(self.saved(), 0)
        self.assertEqual(self.beeps.count(LONG_BEEP), 2)
        self.assertEqual(self.get(self.mot(0, MOT_LOCK)), 0)        # module 0 untouched

    def test_motor_with_a_slot_is_edited_as_before(self):
        self.edit(ITEM_LOCK, yes=True)
        self.call("screen_edss.o:centerPressed")
        self.assertEqual(self.get(self.mot(0, MOT_LOCK)), 1)
        self.assertEqual(self.saved(), 1)

    # KI-7 -----------------------------------------------------------------
    def test_unchanged_lock_or_hide_does_not_rewrite_flash(self):
        self.put(self.mot(0, MOT_LOCK), 1)
        self.edit(ITEM_LOCK, yes=True)
        self.call("screen_edss.o:centerClicked")
        self.edit(ITEM_HIDE, yes=False)
        self.call("screen_edss.o:centerPressed")
        self.assertEqual(self.saved(), 0)

    def test_changed_flag_is_saved(self):
        self.edit(ITEM_HIDE, yes=True)
        self.call("screen_edss.o:centerClicked")
        self.assertEqual(self.get(self.mot(0, MOT_HIDE)), 1)
        self.assertEqual(self.saved(), 1)

    def test_delete_no_on_a_paired_device_does_not_rewrite_flash(self):
        self.edit(ITEM_DELETE, yes=False)
        self.call("screen_edss.o:centerClicked")
        self.assertEqual(self.get(self.cfg(0, CFG_DELETE)), 0)
        self.assertEqual(self.saved(), 0)

    # KI-8 -----------------------------------------------------------------
    def test_pairing_screen_is_drawn_once_when_there_is_no_room(self):
        self.connect_ok = 0
        self.call("screen_dps.o:connect")
        self.assertEqual(self.switches, [(self.screen("devicePairingScreen"), NO_ROOM)])

    def test_pairing_screen_is_drawn_once_when_connecting(self):
        self.call("screen_dps.o:connect")
        self.assertEqual(self.switches, [(self.screen("devicePairingScreen"), CONNECTING)])


if __name__ == "__main__":
    unittest.main(verbosity=2)
