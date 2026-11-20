"""Execute the built ARM visibility helpers in Unicorn, without connecting a board.

Requires: pip install unicorn. Build and run make_package.py first.
Offsets below are checked against AXF DWARF; map addresses are read dynamically.
"""
import os
import re
import subprocess
import struct
import unittest
from pathlib import Path

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                              UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC)

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "Application/MDK-ARM/USB_Application"
MAP = (BUILD / "USB_Application.map").read_text()
FROMELF = Path(os.environ.get("LOCALAPPDATA", "")) / "Keil_v5/ARM/ARM_Compiler_5.06u7/bin/fromelf.exe"


def symbol(name):
    match = re.search(r"^\s+" + re.escape(name) + r"\s+(0x[0-9a-fA-F]+)\s+(?:Data|Thumb Code)\s+(\d+)", MAP, re.M)
    if not match:
        raise AssertionError(f"Missing symbol: {name}")
    return int(match[1], 16), int(match[2])


class FirmwareHarness(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        dwarf = subprocess.check_output([str(FROMELF), "--text", "-g", str(BUILD / "USB_Application.axf")], text=True)
        for field, offsets in {"isHide": {69, 39}, "isLock": {68, 38},
                               "isDelete": {20, 40}, "motorData": {36},
                               "currentDeviceId": {80}, "currentModuleId": {81},
                               "isNeedSaveConfig": {82}, "DeviceConfig": {15}}.items():
            actual = {int(v) for v in re.findall(r"DW_AT_name " + field + r"\s+[^\n]*\n[^\n]*DW_AT_data_member_location[^\n]*DW_OP_plus_uconst (\d+)", dwarf)}
            if actual != offsets:
                raise AssertionError(f"Changed layout {field}: {actual} != {offsets}")
        cls.device, size = symbol("deviceBuff")
        assert size == 20 * 468
        cls.system = symbol("systemInfo")[0]
        cls.storage = symbol("storageData")[0]
        cls.fields = {}
        for name in ["netIdConfig", "frequencyConfig", "sfConfig", "sleepMode", "tUpdate", "tSleepUpdate"]:
            offsets = re.findall(r"DW_AT_name " + name + r"\s+[^\n]*\n[^\n]*DW_AT_data_member_location[^\n]*DW_OP_plus_uconst (\d+)", dwarf)
            cls.fields[name] = max(map(int, offsets))

    def setUp(self):
        self.cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        self.cpu.mem_map(0x08000000, 0x80000)
        self.cpu.mem_write(0x08008000, (ROOT / "dist/application.bin").read_bytes())
        self.cpu.mem_map(0x20000000, 0x10000)
        for i in range(20):
            self.put(self.device + i * 468 + 20, 1)

    def put(self, address, value):
        self.cpu.mem_write(address, bytes([value]))

    def get(self, address):
        return self.cpu.mem_read(address, 1)[0]

    def add(self, index, hidden=False):
        self.put(self.device + index * 468 + 20, 0)
        self.put(self.device + index * 468 + 36 + 69, int(hidden))

    def select(self, index, module=0):
        self.put(self.system + 80, index)
        self.put(self.system + 81, module)

    def call(self, name, argument=0, argument1=0, argument2=0):
        stop = 0x0807FFF0
        self.cpu.reg_write(UC_ARM_REG_SP, 0x2000F000)
        self.cpu.reg_write(UC_ARM_REG_LR, stop | 1)
        self.cpu.reg_write(UC_ARM_REG_R0, argument & 0xFFFFFFFF)
        self.cpu.reg_write(UC_ARM_REG_R1, argument1)
        self.cpu.reg_write(UC_ARM_REG_R2, argument2)
        self.cpu.emu_start(symbol(name)[0] | 1, stop, count=10000)
        self.assertEqual(self.cpu.reg_read(UC_ARM_REG_PC), stop)
        return self.cpu.reg_read(UC_ARM_REG_R0)

class VisibilityTests(FirmwareHarness):
    def test_visible_hidden_deleted_and_invalid(self):
        self.add(0)
        self.add(1, True)
        self.assertEqual([self.call("Screen_DeviceVisible", i) for i in [0, 1, 2, 20, 255]], [1, 0, 0, 0, 0])

    def test_hide_m11_selects_m21(self):
        self.add(0, True)
        self.add(1)
        self.select(0, 2)
        self.assertEqual(self.call("Screen_EnsureVisibleDevice"), 1)
        self.assertEqual(self.get(self.system + 80), 1)
        self.assertEqual(self.get(self.system + 81), 0)

    def test_hide_last_falls_back_to_previous(self):
        self.add(0)
        self.add(7)
        self.add(19, True)
        self.select(19)
        self.assertEqual(self.call("Screen_EnsureVisibleDevice"), 1)
        self.assertEqual(self.get(self.system + 80), 7)

    def test_all_hidden_safe_indices(self):
        self.add(0, True)
        self.add(1, True)
        self.select(1)
        self.assertEqual(self.call("Screen_EnsureVisibleDevice"), 0)
        self.assertEqual(self.get(self.system + 80), 0)
        self.assertEqual(self.get(self.system + 81), 0)

    def test_empty_and_restore_visibility(self):
        self.assertEqual(self.call("Screen_EnsureVisibleDevice"), 0)
        self.add(3)
        self.assertEqual(self.call("Screen_EnsureVisibleDevice"), 1)
        self.assertEqual(self.get(self.system + 80), 3)

    def test_next_previous_skip_hidden_and_deleted(self):
        self.add(0)
        self.add(1, True)
        self.add(3)
        self.select(0)
        self.call("Screen_SelectAdjacentVisible", 1)
        self.assertEqual(self.get(self.system + 80), 3)
        self.call("Screen_SelectAdjacentVisible", -1)
        self.assertEqual(self.get(self.system + 80), 0)

    def test_navigation_boundaries_no_underflow(self):
        self.add(0)
        self.select(0)
        self.call("Screen_SelectAdjacentVisible", -1)
        self.assertEqual(self.get(self.system + 80), 0)
        self.call("Screen_SelectAdjacentVisible", 1)
        self.assertEqual(self.get(self.system + 80), 0)

    def test_invalid_start_recovered(self):
        self.add(2)
        self.select(255)
        self.assertEqual(self.call("Screen_EnsureVisibleDevice"), 1)
        self.assertEqual(self.get(self.system + 80), 2)

    def test_save_flags_updates_only_flags_and_marks_dirty(self):
        self.add(2, True)
        self.put(self.device + 2 * 468 + 36 + 68, 1)
        before = bytes(self.cpu.mem_read(self.storage, 836))
        self.call("Screen_SaveDeviceFlags", 2)
        expected = bytearray(before)
        expected[15 + 2 * 41 + 38:15 + 2 * 41 + 40] = b"\x01\x01"
        self.assertEqual(bytes(self.cpu.mem_read(self.storage, 836)), bytes(expected))
        self.assertEqual(self.get(self.system + 82), 1)
        self.put(self.system + 82, 0)
        self.call("Screen_SaveDeviceFlags", 2)
        self.assertEqual(self.get(self.system + 82), 0)

    def test_save_invalid_id_does_not_modify_storage(self):
        before = bytes(self.cpu.mem_read(self.storage, 836))
        self.call("Screen_SaveDeviceFlags", 255)
        self.assertEqual(bytes(self.cpu.mem_read(self.storage, 836)), before)
        self.assertEqual(self.get(self.system + 82), 0)

    def test_device_name_delete_and_alarm_staged_for_flash(self):
        self.add(2)
        base = self.device + 2 * 468
        # MotorData name/alarm offsets are fixed by the current checked layout.
        self.cpu.mem_write(base + 36 + 40, b"Pump test\x00" + bytes(10))
        self.cpu.mem_write(base + 36 + 60, struct.pack("<f", 12.34))
        self.cpu.mem_write(base + 36 + 64, struct.pack("<I", 30))
        self.put(base + 20, 1)
        self.call("Screen_SaveDeviceSettings", 2)
        target = self.storage + 15 + 2 * 41
        self.assertEqual(bytes(self.cpu.mem_read(target + 10, 10)), b"Pump test\x00")
        self.assertEqual(bytes(self.cpu.mem_read(target + 30, 4)), struct.pack("<f", 12.34))
        self.assertEqual(bytes(self.cpu.mem_read(target + 34, 4)), struct.pack("<I", 30))
        self.assertEqual(self.get(target + 40), 1)
        self.assertEqual(self.get(self.system + 82), 1)
        self.assertEqual(self.call("Screen_DeviceVisible", 2), 0)

    def test_hidden_blocks_power_and_schedule_send(self):
        self.add(0, True)
        self.select(0)
        def reject_queue(cpu, address, size, data):
            self.fail("Hidden device attempted to send a LoRa command")
        address = symbol("osMessageQueuePut")[0] & ~1
        self.cpu.hook_add(UC_HOOK_CODE, reject_queue, begin=address, end=address)
        self.call("Main_SetMotorPower", 1)
        self.call("Main_SetMotorPower", 2)
        self.call("Main_SetScheduleTimeToCurrentModule", 1800)

    def test_general_settings_saved_without_periodic_poll(self):
        values = {"netIdConfig": (2, 1), "frequencyConfig": (434, 4), "sfConfig": (8, 1),
                  "sleepMode": (1, 1), "tUpdate": (6, 4), "tSleepUpdate": (301, 4)}
        for name, (value, size) in values.items():
            self.cpu.mem_write(self.system + self.fields[name], value.to_bytes(size, "little"))
        self.cpu.mem_write(symbol("GetDevStatusTimerHandle")[0], struct.pack("<I", 0x20008000))
        calls = []
        def timer(cpu, address, size, data):
            calls.append(cpu.reg_read(UC_ARM_REG_R0))
            cpu.reg_write(UC_ARM_REG_R0, 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        address = symbol("osTimerStop")[0] & ~1
        self.cpu.hook_add(UC_HOOK_CODE, timer, begin=address, end=address)
        self.call("Screen_SaveGeneralSettings")
        self.assertEqual(bytes(self.cpu.mem_read(self.storage, 15)), struct.pack("<BIBBII", 2, 434, 8, 1, 6, 301))
        self.assertEqual(self.get(self.system + 82), 1)
        self.assertEqual(calls, [0x20008000])
        self.assertNotRegex(MAP, r"^\s+osTimerStart\s+0x", "Unconditional polling timer must stay unlinked")

    def test_manager_can_restore_hidden_without_changing_selection_or_lock(self):
        self.add(2, True)
        self.put(self.device + 2 * 468 + 36 + 68, 1)
        self.select(0)
        self.put(symbol("managerCount")[0], 1)
        self.put(symbol("managerSelected")[0], 0)
        self.put(symbol("managerIds")[0], 2)
        self.call("Manager_Toggle")
        self.assertEqual(self.call("Screen_DeviceVisible", 2), 1)
        self.assertEqual(self.get(self.system + 80), 0)
        self.assertEqual(self.get(self.device + 2 * 468 + 36 + 68), 1)
        self.assertEqual(self.get(self.storage + 15 + 2 * 41 + 39), 0)
        self.assertEqual(self.get(self.system + 82), 1)

    def test_manager_navigation_and_empty_toggle(self):
        selected = symbol("managerSelected")[0]
        self.put(symbol("managerCount")[0], 20)
        self.put(selected, 0)
        self.call("Manager_Previous")
        self.assertEqual(self.get(selected), 0)
        for _ in range(25):
            self.call("Manager_Next")
        self.assertEqual(self.get(selected), 19)
        self.put(symbol("managerCount")[0], 0)
        before = bytes(self.cpu.mem_read(self.storage, 836))
        self.call("Manager_Toggle")
        self.assertEqual(bytes(self.cpu.mem_read(self.storage, 836)), before)

    def flash_mocks(self, erase_error=False, program_error=False):
        calls = []
        names = ["HAL_FLASH_Unlock", "HAL_FLASH_Lock", "HAL_FLASHEx_Erase",
                 "HAL_FLASH_Program", "HAL_FLASH_GetError"]
        hooks = {symbol(name)[0] & ~1: name for name in names}
        def intercept(cpu, address, size, data):
            name = hooks.get(address)
            if name is None:
                return
            result = 0
            if name == "HAL_FLASHEx_Erase":
                config = cpu.reg_read(UC_ARM_REG_R0)
                page, count = struct.unpack("<II", bytes(cpu.mem_read(config + 8, 8)))
                calls.append((name, page, count))
                result = int(erase_error)
            elif name == "HAL_FLASH_Program":
                calls.append((name, cpu.reg_read(UC_ARM_REG_R1), cpu.reg_read(UC_ARM_REG_R2)))
                result = int(program_error)
            else:
                calls.append((name,))
                if name == "HAL_FLASH_GetError":
                    result = 4
            cpu.reg_write(UC_ARM_REG_R0, result)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE, intercept)
        return calls

    def test_flash_byte_count_not_word_count(self):
        calls = self.flash_mocks()
        source = 0x20008000
        payload = bytes(i % 256 for i in range(836))
        self.cpu.mem_write(source, payload + b"BAD!")
        self.assertEqual(self.call("writeDataToFlash", 0x0803F000, source, len(payload)), 0)
        self.assertIn(("HAL_FLASHEx_Erase", 0x0803F000, 1), calls)
        writes = [c for c in calls if c[0] == "HAL_FLASH_Program"]
        self.assertEqual(len(writes), 209)
        self.assertEqual([c[1] for c in writes], list(range(0x0803F000, 0x0803F000 + 836, 4)))
        self.assertEqual(b"".join(struct.pack("<I", c[2]) for c in writes), payload)
        self.assertEqual(calls[-1], ("HAL_FLASH_Lock",))

    def test_flash_partial_word_padding(self):
        calls = self.flash_mocks()
        self.cpu.mem_write(0x20008000, b"ABCDEBAD")
        self.call("writeDataToFlash", 0x0803F000, 0x20008000, 5)
        writes = [c for c in calls if c[0] == "HAL_FLASH_Program"]
        self.assertEqual([c[2] for c in writes], [0x44434241, 0xFFFFFF45])

    def test_flash_relocks_on_erase_failure(self):
        calls = self.flash_mocks(erase_error=True)
        self.assertEqual(self.call("writeDataToFlash", 0x0803F000, 0x20008000, 836), 4)
        self.assertIn(("HAL_FLASH_Lock",), calls)
        self.assertFalse(any(c[0] == "HAL_FLASH_Program" for c in calls))

    def test_flash_relocks_on_program_failure(self):
        calls = self.flash_mocks(program_error=True)
        self.assertEqual(self.call("writeDataToFlash", 0x0803F000, 0x20008000, 836), 4)
        self.assertIn(("HAL_FLASH_Lock",), calls)
        self.assertEqual(len([c for c in calls if c[0] == "HAL_FLASH_Program"]), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
