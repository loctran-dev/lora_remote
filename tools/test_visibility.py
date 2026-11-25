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
    """Return (address, size) of a map symbol.

    Use "file.o:name" for a static symbol whose name exists in several objects,
    e.g. "screen_dss.o:init".
    """
    obj, _, sym = name.rpartition(":")
    owner = r"\s+" + re.escape(obj) + r"\(" if obj else ""
    match = re.search(r"^\s+" + re.escape(sym) + r"\s+(0x[0-9a-fA-F]+)\s+(?:Data|Thumb Code)\s+(\d+)" + owner,
                      MAP, re.M)
    if not match:
        raise AssertionError(f"Missing symbol: {name}")
    return int(match[1], 16), int(match[2])


# DeviceInfo_t (runtime state only) and its first MotorData_t.
DEVICE_STRIDE = 192       # sizeof(DeviceInfo_t)
DEV_CONNECTION_ERROR = 20
DEV_RSSI = 8
DEV_MODULE = 24           # motorData[0]
MODULE_STRIDE = 28        # sizeof(MotorData_t)
# DeviceStorageData_t (packed), storageData.DeviceConfig[] at storage + 15: device identity.
CFG_BASE, CFG_STRIDE = 15, 10
CFG_NODE, CFG_SERIAL, CFG_TYPE, CFG_TYPE_CHAR, CFG_MODULES, CFG_DELETE = 0, 1, 5, 6, 7, 9
# MotorStorageData_t (packed), storageData.MotorConfig[40] at storage + 215: settings of one motor.
MOTOR_BASE, MOTOR_STRIDE, MOTOR_SLOTS, MOTOR_FREE = 215, 32, 40, 0xFF
MOT_DEVICE, MOT_MODULE, MOT_NAME, MOT_THRESHOLD, MOT_DELAY, MOT_LOCK, MOT_HIDE = 0, 1, 2, 22, 26, 30, 31
STORAGE_SIZE = 1545       # sizeof(SystemStorageData_t)
AUTO_STATUS = 1504        # storageData.autoStatusSeconds[]


class FirmwareHarness(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        dwarf = subprocess.check_output([str(FROMELF), "--text", "-g", str(BUILD / "USB_Application.axf")], text=True)
        # autoStatusSeconds also sits at offset 5 of PayloadRequestSetConfig_t.
        for field, offsets in {"isHide": {MOT_HIDE}, "isLock": {MOT_LOCK},
                               "isDelete": {CFG_DELETE}, "motorData": {DEV_MODULE},
                               "MotorConfig": {MOTOR_BASE}, "autoStatusSeconds": {5, AUTO_STATUS},
                               "isConnectionError": {DEV_CONNECTION_ERROR}, "rssi": {DEV_RSSI},
                               "currentDeviceId": {60}, "currentModuleId": {61},
                               "isNeedSaveConfig": {62}, "DeviceConfig": {15}}.items():
            actual = {int(v) for v in re.findall(r"DW_AT_name " + field + r"\s+[^\n]*\n[^\n]*DW_AT_data_member_location[^\n]*DW_OP_plus_uconst (\d+)", dwarf)}
            if actual != offsets:
                raise AssertionError(f"Changed layout {field}: {actual} != {offsets}")
        cls.device, size = symbol("deviceBuff")
        assert size == 20 * DEVICE_STRIDE
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
            self.put(self.cfg(i, CFG_DELETE), 1)
        for slot in range(MOTOR_SLOTS):
            self.put(self.mot(slot, MOT_DEVICE), MOTOR_FREE)

    def put(self, address, value):
        self.cpu.mem_write(address, bytes([value]))

    def get(self, address):
        return self.cpu.mem_read(address, 1)[0]

    def dev(self, index, offset=0):
        """Address in deviceBuff[index] (runtime state)."""
        return self.device + index * DEVICE_STRIDE + offset

    def cfg(self, index, offset=0):
        """Address in storageData.DeviceConfig[index] (identity and settings)."""
        return self.storage + CFG_BASE + index * CFG_STRIDE + offset

    def mot(self, slot, offset=0):
        """Address in storageData.MotorConfig[slot]; add(i) gives device i's module 0 slot i."""
        return self.storage + MOTOR_BASE + slot * MOTOR_STRIDE + offset

    def add(self, index, hidden=False):
        """Pair device index with one motor, held in motor slot index."""
        self.put(self.cfg(index, CFG_DELETE), 0)
        self.put(self.mot(index, MOT_DEVICE), index)
        self.put(self.mot(index, MOT_MODULE), 0)
        self.put(self.mot(index, MOT_HIDE), int(hidden))

    def select(self, index, module=0):
        self.put(self.system + 60, index)
        self.put(self.system + 61, module)

    def call(self, name, argument=0, argument1=0, argument2=0, count=10000):
        stop = 0x0807FFF0
        self.cpu.reg_write(UC_ARM_REG_SP, 0x2000F000)
        self.cpu.reg_write(UC_ARM_REG_LR, stop | 1)
        self.cpu.reg_write(UC_ARM_REG_R0, argument & 0xFFFFFFFF)
        self.cpu.reg_write(UC_ARM_REG_R1, argument1)
        self.cpu.reg_write(UC_ARM_REG_R2, argument2)
        self.cpu.emu_start(symbol(name)[0] | 1, stop, count=count)
        self.assertEqual(self.cpu.reg_read(UC_ARM_REG_PC), stop)
        return self.cpu.reg_read(UC_ARM_REG_R0)

class VisibilityTests(FirmwareHarness):
    def test_visible_hidden_deleted_and_invalid(self):
        self.add(0)
        self.add(1, True)
        self.assertEqual([self.call("ScrnCommon_DeviceVisible", i) for i in [0, 1, 2, 20, 255]], [1, 0, 0, 0, 0])

    def test_hide_m11_selects_m21(self):
        self.add(0, True)
        self.add(1)
        self.select(0, 2)
        self.assertEqual(self.call("ScrnCommon_EnsureVisibleDevice"), 1)
        self.assertEqual(self.get(self.system + 60), 1)
        self.assertEqual(self.get(self.system + 61), 0)

    def test_hide_last_falls_back_to_previous(self):
        self.add(0)
        self.add(7)
        self.add(19, True)
        self.select(19)
        self.assertEqual(self.call("ScrnCommon_EnsureVisibleDevice"), 1)
        self.assertEqual(self.get(self.system + 60), 7)

    def test_all_hidden_safe_indices(self):
        self.add(0, True)
        self.add(1, True)
        self.select(1)
        self.assertEqual(self.call("ScrnCommon_EnsureVisibleDevice"), 0)
        self.assertEqual(self.get(self.system + 60), 0)
        self.assertEqual(self.get(self.system + 61), 0)

    def test_empty_and_restore_visibility(self):
        self.assertEqual(self.call("ScrnCommon_EnsureVisibleDevice"), 0)
        self.add(3)
        self.assertEqual(self.call("ScrnCommon_EnsureVisibleDevice"), 1)
        self.assertEqual(self.get(self.system + 60), 3)

    def test_next_previous_skip_hidden_and_deleted(self):
        self.add(0)
        self.add(1, True)
        self.add(3)
        self.select(0)
        self.call("ScrnCommon_SelectAdjacentVisible", 1)
        self.assertEqual(self.get(self.system + 60), 3)
        self.call("ScrnCommon_SelectAdjacentVisible", -1)
        self.assertEqual(self.get(self.system + 60), 0)

    def test_navigation_boundaries_no_underflow(self):
        self.add(0)
        self.select(0)
        self.call("ScrnCommon_SelectAdjacentVisible", -1)
        self.assertEqual(self.get(self.system + 60), 0)
        self.call("ScrnCommon_SelectAdjacentVisible", 1)
        self.assertEqual(self.get(self.system + 60), 0)

    def test_invalid_start_recovered(self):
        self.add(2)
        self.select(255)
        self.assertEqual(self.call("ScrnCommon_EnsureVisibleDevice"), 1)
        self.assertEqual(self.get(self.system + 60), 2)

    def test_save_device_settings_only_marks_dirty(self):
        # Editors write storageData.DeviceConfig directly; saving only requests the flash write.
        self.add(2, True)
        self.put(self.mot(2, MOT_LOCK), 1)
        before = bytes(self.cpu.mem_read(self.storage, STORAGE_SIZE))
        self.call("ScrnCommon_SaveDeviceSettings")
        self.assertEqual(bytes(self.cpu.mem_read(self.storage, STORAGE_SIZE)), before)
        self.assertEqual(self.get(self.system + 62), 1)

    def test_deleted_device_is_no_longer_visible(self):
        self.add(2)
        self.assertEqual(self.call("ScrnCommon_DeviceVisible", 2), 1)
        self.put(self.cfg(2, CFG_DELETE), 1)
        self.assertEqual(self.call("ScrnCommon_DeviceVisible", 2), 0)

    def test_device_settings_have_no_runtime_copy(self):
        # Runtime state must not change when settings are edited, and vice versa.
        self.add(2)
        runtime = bytes(self.cpu.mem_read(self.dev(2), DEVICE_STRIDE))
        self.cpu.mem_write(self.mot(2, MOT_NAME), b"Pump test\x00")
        self.cpu.mem_write(self.mot(2, MOT_THRESHOLD), struct.pack("<fI", 12.34, 30))
        self.call("ScrnCommon_SaveDeviceSettings")
        self.assertEqual(bytes(self.cpu.mem_read(self.dev(2), DEVICE_STRIDE)), runtime)

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
            self.cpu.mem_write(self.storage + self.fields[name], value.to_bytes(size, "little"))
        self.cpu.mem_write(symbol("GetDevStatusTimerHandle")[0], struct.pack("<I", 0x20008000))
        calls = []
        def timer(cpu, address, size, data):
            calls.append(cpu.reg_read(UC_ARM_REG_R0))
            cpu.reg_write(UC_ARM_REG_R0, 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        address = symbol("osTimerStop")[0] & ~1
        self.cpu.hook_add(UC_HOOK_CODE, timer, begin=address, end=address)
        self.call("ScrnCommon_SaveGeneralSettings")
        self.assertEqual(bytes(self.cpu.mem_read(self.storage, 15)), struct.pack("<BIBBII", 2, 434, 8, 1, 6, 301))
        self.assertEqual(self.get(self.system + 62), 1)
        self.assertEqual(calls, [0x20008000])
        self.assertNotRegex(MAP, r"^\s+osTimerStart\s+0x", "Unconditional polling timer must stay unlinked")

    def test_manager_can_restore_hidden_without_changing_selection_or_lock(self):
        self.add(2, True)
        self.put(self.mot(2, MOT_LOCK), 1)
        self.select(0)
        self.put(symbol("managerCount")[0], 1)
        self.put(symbol("managerSelected")[0], 0)
        self.put(symbol("managerIds")[0], 2)
        self.call("screen_dvm.o:centerClicked")
        self.assertEqual(self.call("ScrnCommon_DeviceVisible", 2), 1)
        self.assertEqual(self.get(self.system + 60), 0)
        self.assertEqual(self.get(self.mot(2, MOT_LOCK)), 1)
        self.assertEqual(self.get(self.mot(2, MOT_HIDE)), 0)
        self.assertEqual(self.get(self.system + 62), 1)

    def test_manager_navigation_and_empty_toggle(self):
        selected = symbol("managerSelected")[0]
        self.put(symbol("managerCount")[0], 20)
        self.put(selected, 0)
        self.call("screen_dvm.o:leftClicked")
        self.assertEqual(self.get(selected), 0)
        for _ in range(25):
            self.call("screen_dvm.o:rightClicked")
        self.assertEqual(self.get(selected), 19)
        self.put(symbol("managerCount")[0], 0)
        before = bytes(self.cpu.mem_read(self.storage, STORAGE_SIZE))
        self.call("screen_dvm.o:centerClicked")
        self.assertEqual(bytes(self.cpu.mem_read(self.storage, STORAGE_SIZE)), before)

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

    # Flash_WriteData() has no caller since config_store, so the linker drops it;
    # Config_Save() uses Flash_Erase() + Flash_Program(), which are checked here.
    def test_flash_byte_count_not_word_count(self):
        calls = self.flash_mocks()
        source = 0x20008000
        payload = bytes(i % 256 for i in range(836))
        self.cpu.mem_write(source, payload + b"BAD!")
        self.assertEqual(self.call("Flash_Erase", 0x0803F000, len(payload)), 0)
        self.assertEqual(self.call("Flash_Program", 0x0803F000, source, len(payload)), 0)
        self.assertIn(("HAL_FLASHEx_Erase", 0x0803F000, 1), calls)
        writes = [c for c in calls if c[0] == "HAL_FLASH_Program"]
        self.assertEqual(len(writes), 209)
        self.assertEqual([c[1] for c in writes], list(range(0x0803F000, 0x0803F000 + 836, 4)))
        self.assertEqual(b"".join(struct.pack("<I", c[2]) for c in writes), payload)
        self.assertEqual(calls[-1], ("HAL_FLASH_Lock",))

    def test_flash_partial_word_padding(self):
        calls = self.flash_mocks()
        self.cpu.mem_write(0x20008000, b"ABCDEBAD")
        self.call("Flash_Program", 0x0803F000, 0x20008000, 5)
        writes = [c for c in calls if c[0] == "HAL_FLASH_Program"]
        self.assertEqual([c[2] for c in writes], [0x44434241, 0xFFFFFF45])

    def test_flash_relocks_on_erase_failure(self):
        calls = self.flash_mocks(erase_error=True)
        self.assertEqual(self.call("Flash_Erase", 0x0803F000, 836), 4)
        self.assertIn(("HAL_FLASH_Lock",), calls)
        self.assertFalse(any(c[0] == "HAL_FLASH_Program" for c in calls))

    def test_flash_relocks_on_program_failure(self):
        calls = self.flash_mocks(program_error=True)
        self.assertEqual(self.call("Flash_Program", 0x0803F000, 0x20008000, 836), 4)
        self.assertIn(("HAL_FLASH_Lock",), calls)
        self.assertEqual(len([c for c in calls if c[0] == "HAL_FLASH_Program"]), 1)

if __name__ == "__main__":
    unittest.main(verbosity=2)
