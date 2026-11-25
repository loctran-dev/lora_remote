"""Built-ARM Config_Load/Config_Save checks with emulated flash: CFG3 format, motor pool and the
conversion of older blocks (CFG2 slots and the raw single-copy format). No board access."""
import struct
import unittest

import test_visibility as h
from test_visibility import (CFG_DELETE, CFG_MODULES, CFG_NODE, CFG_SERIAL, MOT_DELAY, MOT_DEVICE, MOT_HIDE,
                             MOT_LOCK, MOT_MODULE, MOT_NAME, MOT_THRESHOLD, MOTOR_FREE, MOTOR_SLOTS, STORAGE_SIZE)
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_PC, UC_ARM_REG_LR

SLOT_A, SLOT_B, SLOT_SIZE = 0x0803F000, 0x0803F800, 0x800
CFG2, CFG3 = 0x32474643, 0x33474643
V2_SIZE = 885
SCRATCH = 0x20008000
LINK_MAGIC = 0x314B4E4C   # link_monitor.c, "LNK1"


def v2_device(node=0, serial=0, modules=1, name=b"", threshold=0.0, delay=0, lock=0, hide=0, deleted=1):
    """One ConfigV2Device_t (41 bytes); FactoryInfo_t is 8 bytes (serial, type, letter, modules, pad)."""
    return struct.pack("<BBIBBBx20sfIBBB", node, 0, serial, 1, ord("M"), modules, name, threshold, delay,
                       lock, hide, deleted)


def v2_block(devices, net=2, freq=434, sf=8, sleep=1, update=6, sleep_update=301, link=None):
    """A ConfigV2_t (885 bytes). link = (magic, idle, timeout, auto[20], checksum)."""
    devices = devices + [v2_device()] * (20 - len(devices))
    magic, idle, timeout, auto, checksum = link or (0, 0, 0, [0] * 20, 0)
    block = (struct.pack("<BiBBII", net, freq, sf, sleep, update, sleep_update) + b"".join(devices) +
             struct.pack("<BIHH20HB", 1, magic, idle, timeout, *auto, checksum))
    assert len(block) == V2_SIZE
    return block


class ConfigMigrationTests(h.FirmwareHarness):
    def setUp(self):
        super().setUp()
        self.cpu.mem_write(SLOT_A, b"\xff" * 2 * SLOT_SIZE)
        self.erases = []
        names = ["HAL_FLASH_Unlock", "HAL_FLASH_Lock", "HAL_FLASH_Program", "HAL_FLASHEx_Erase", "HAL_FLASH_GetError"]
        hooks = {h.symbol(n)[0] & ~1: n for n in names}

        def flash(cpu, address, size, data):
            name = hooks.get(address)
            if name is None:
                return
            if name == "HAL_FLASHEx_Erase":
                page, count = struct.unpack("<II", bytes(cpu.mem_read(cpu.reg_read(UC_ARM_REG_R0) + 8, 8)))
                self.erases.append(page)
                cpu.mem_write(page, b"\xff" * SLOT_SIZE * count)
            elif name == "HAL_FLASH_Program":
                cpu.mem_write(cpu.reg_read(UC_ARM_REG_R1), struct.pack("<I", cpu.reg_read(UC_ARM_REG_R2)))
            cpu.reg_write(UC_ARM_REG_R0, 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE, flash)

    # Helpers -------------------------------------------------------------
    def crc32(self, payload):
        """CRC-32 as the firmware computes it (Util_Crc32)."""
        self.cpu.mem_write(SCRATCH, payload)
        return self.fw("Util_Crc32", SCRATCH, len(payload))

    def link_checksum(self, magic, idle, timeout, auto):
        self.cpu.mem_write(SCRATCH, struct.pack("<IHH20H", magic, idle, timeout, *auto))
        return self.fw("Util_CalculateCRC", SCRATCH, 48) & 0xFF

    def write_slot(self, address, magic, sequence, payload):
        header = struct.pack("<IHHII", sequence, len(payload), 0xFFFF, self.crc32(payload), magic)
        self.cpu.mem_write(address, header + payload)

    def stored(self, offset, size=1):
        """Bytes of storageData."""
        return bytes(self.cpu.mem_read(self.storage + offset, size))

    def fw(self, name, *args):
        """Call with room for the CRC-32 loops of a whole slot."""
        return self.call(name, *args, count=5_000_000)

    def free_slots(self):
        return sum(self.get(self.mot(s, MOT_DEVICE)) == MOTOR_FREE for s in range(MOTOR_SLOTS))

    def motors(self, device):
        return {self.get(self.mot(s, MOT_MODULE)): s for s in range(MOTOR_SLOTS)
                if self.get(self.mot(s, MOT_DEVICE)) == device}

    def text(self, address):
        return bytes(self.cpu.mem_read(address, 20)).split(b"\0")[0].decode()

    def slot_magic(self, address):
        return struct.unpack("<I", bytes(self.cpu.mem_read(address + 12, 4)))[0]

    def slot_sequence(self, address):
        return struct.unpack("<I", bytes(self.cpu.mem_read(address, 4)))[0]

    def legacy_link(self):
        auto = [60] * 20
        auto[0] = 2
        return (LINK_MAGIC, 45, 180, auto, self.link_checksum(LINK_MAGIC, 45, 180, auto))

    def old_devices(self):
        return [v2_device(node=1, serial=0xABCDEF01, name=b"Pump A", threshold=1.5, delay=30, lock=1, hide=0,
                          deleted=0),
                v2_device(),                                            # free slot
                v2_device(node=3, serial=0x12345678, modules=3, name=b"Fan", hide=1, deleted=0)]

    def assert_converted(self):
        self.assertEqual(self.stored(0, 15), struct.pack("<BiBBII", 2, 434, 8, 1, 6, 301))
        self.assertEqual((self.get(self.cfg(0, CFG_NODE)), self.get(self.cfg(0, CFG_DELETE))), (1, 0))
        self.assertEqual(struct.unpack("<I", bytes(self.cpu.mem_read(self.cfg(0, CFG_SERIAL), 4)))[0], 0xABCDEF01)
        self.assertEqual(self.get(self.cfg(1, CFG_DELETE)), 1)
        self.assertEqual(self.get(self.cfg(2, CFG_MODULES)), 3)
        # Device 0: one motor carrying the old settings.
        a = self.motors(0)
        self.assertEqual(list(a), [0])
        self.assertEqual(self.text(self.mot(a[0], MOT_NAME)), "Pump A")
        self.assertEqual(bytes(self.cpu.mem_read(self.mot(a[0], MOT_THRESHOLD), 8)), struct.pack("<fI", 1.5, 30))
        self.assertEqual((self.get(self.mot(a[0], MOT_LOCK)), self.get(self.mot(a[0], MOT_HIDE))), (1, 0))
        # Device 2: three motors, module 0 has the old settings, the others the defaults.
        c = self.motors(2)
        self.assertEqual(sorted(c), [0, 1, 2])
        self.assertEqual(self.text(self.mot(c[0], MOT_NAME)), "Fan")
        self.assertEqual(self.get(self.mot(c[0], MOT_HIDE)), 1)
        for module in (1, 2):
            self.assertEqual(self.text(self.mot(c[module], MOT_NAME)), "Default")
            self.assertEqual(self.get(self.mot(c[module], MOT_HIDE)), 0)
        self.assertEqual(self.motors(1), {})
        self.assertEqual(self.free_slots(), MOTOR_SLOTS - 4)

    def assert_link_settings_kept(self):
        self.call("Link_Init", 0)
        self.assertEqual(self.call("Link_GetIdleSeconds") & 0xFFFF, 45)
        self.assertEqual(self.call("Link_GetAutoStatus", 0) & 0xFFFF, 2)

    # Tests ---------------------------------------------------------------
    def test_cfg2_slot_is_converted_and_next_save_goes_to_the_other_slot(self):
        old = v2_block(self.old_devices(), link=self.legacy_link())
        self.write_slot(SLOT_A, CFG2, 4, v2_block([]))                  # older CFG2 copy
        self.write_slot(SLOT_B, CFG2, 5, old)
        self.fw("Config_Load")
        self.assert_converted()
        self.assertEqual(self.erases, [])                               # nothing written on load
        self.assertEqual(self.fw("Config_Save"), 0)
        self.assertEqual((self.slot_magic(SLOT_A), self.slot_sequence(SLOT_A)), (CFG3, 6))
        self.assertEqual(bytes(self.cpu.mem_read(SLOT_B + 16, V2_SIZE)), old)  # CFG2 copy kept
        saved = self.stored(0, STORAGE_SIZE)
        self.cpu.mem_write(self.storage, b"\xaa" * STORAGE_SIZE)
        self.fw("Config_Load")
        self.assertEqual(self.stored(0, STORAGE_SIZE), saved)          # CFG3 wins over CFG2
        self.assert_link_settings_kept()

    def test_raw_single_copy_format_is_converted(self):
        self.cpu.mem_write(SLOT_A, v2_block(self.old_devices(), link=self.legacy_link()))
        self.fw("Config_Load")
        self.assert_converted()
        self.assert_link_settings_kept()
        self.assertEqual(self.fw("Config_Save"), 0)
        self.assertEqual(self.slot_magic(SLOT_B), CFG3)                 # raw copy in slot A kept

    def test_conversion_gives_every_device_its_first_motor_before_extra_modules(self):
        devices = [v2_device(node=i + 1, serial=i + 1, modules=6, name=b"M%d" % i, deleted=0) for i in range(20)]
        self.write_slot(SLOT_A, CFG2, 1, v2_block(devices))
        self.fw("Config_Load")
        for i in range(20):
            motors = self.motors(i)
            self.assertIn(0, motors, i)
            self.assertEqual(self.text(self.mot(motors[0], MOT_NAME)), "M%d" % i)
        self.assertEqual(self.free_slots(), 0)

    def test_blank_flash_loads_defaults_and_saves_cfg3(self):
        self.fw("Config_Load")
        self.assertEqual(self.stored(0, 1), b"\x01")
        self.assertTrue(all(self.get(self.cfg(i, CFG_DELETE)) == 1 for i in range(20)))
        self.assertEqual(self.free_slots(), MOTOR_SLOTS)
        self.assertEqual(self.slot_magic(SLOT_A), CFG3)

    def test_cfg3_inconsistencies_are_repaired_on_load(self):
        self.fw("Config_Load")                                        # defaults
        self.put(self.cfg(0, CFG_DELETE), 0)                            # device 0 paired, no motor yet
        self.put(self.cfg(1, CFG_DELETE), 0)
        for slot, (device, module) in enumerate([(1, 0), (1, 0), (5, 0), (1, 9)]):
            self.put(self.mot(slot, MOT_DEVICE), device)                # duplicate, deleted owner, bad module
            self.put(self.mot(slot, MOT_MODULE), module)
            self.put(self.mot(slot, MOT_LOCK), 7)
        self.assertEqual(self.fw("Config_Save"), 0)
        self.fw("Config_Load")
        self.assertEqual(self.motors(1), {0: 0})
        self.assertEqual(self.get(self.mot(0, MOT_LOCK)), 0)             # flags normalised (7 != TRUE)
        self.assertEqual(list(self.motors(0)), [0])                      # missing module 0 recreated
        self.assertEqual(self.motors(5), {})
        self.assertEqual(self.free_slots(), MOTOR_SLOTS - 2)

    def test_interrupted_cfg3_save_keeps_the_previous_slot(self):
        self.write_slot(SLOT_B, CFG2, 5, v2_block(self.old_devices(), link=self.legacy_link()))
        self.fw("Config_Load")
        payload = self.stored(0, STORAGE_SIZE)
        header = struct.pack("<IHHI", 6, STORAGE_SIZE, 0xFFFF, self.crc32(payload))  # magic never written
        self.cpu.mem_write(SLOT_A, header + b"\xff\xff\xff\xff" + payload)
        self.cpu.mem_write(self.storage, b"\xaa" * STORAGE_SIZE)
        self.fw("Config_Load")
        self.assert_converted()


if __name__ == "__main__":
    unittest.main(verbosity=2)
