"""Built-ARM Config_Load/Config_Save checks with emulated flash: CFG3 slots and the motor pool.
No board access."""
import struct
import unittest

import test_visibility as h
from test_visibility import (CFG_DELETE, MOT_DEVICE, MOT_LOCK, MOT_MODULE, MOT_NAME, MOTOR_FREE, MOTOR_SLOTS,
                             STORAGE_SIZE)
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_PC, UC_ARM_REG_LR

SLOT_A, SLOT_B, SLOT_SIZE = 0x0803F000, 0x0803F800, 0x800
CFG2, CFG3 = 0x32474643, 0x33474643
SCRATCH = 0x20008000


class ConfigLoadTests(h.FirmwareHarness):
    def setUp(self):
        super().setUp()
        self.cpu.mem_write(SLOT_A, b"\xff" * 2 * SLOT_SIZE)
        names = ["HAL_FLASH_Unlock", "HAL_FLASH_Lock", "HAL_FLASH_Program", "HAL_FLASHEx_Erase", "HAL_FLASH_GetError"]
        hooks = {h.symbol(n)[0] & ~1: n for n in names}

        def flash(cpu, address, size, data):
            name = hooks.get(address)
            if name is None:
                return
            if name == "HAL_FLASHEx_Erase":
                page, count = struct.unpack("<II", bytes(cpu.mem_read(cpu.reg_read(UC_ARM_REG_R0) + 8, 8)))
                cpu.mem_write(page, b"\xff" * SLOT_SIZE * count)
            elif name == "HAL_FLASH_Program":
                cpu.mem_write(cpu.reg_read(UC_ARM_REG_R1), struct.pack("<I", cpu.reg_read(UC_ARM_REG_R2)))
            cpu.reg_write(UC_ARM_REG_R0, 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE, flash)

    def fw(self, name, *args):
        """Call with room for the CRC-32 loops of a whole slot."""
        return self.call(name, *args, count=5_000_000)

    def crc32(self, payload):
        """CRC-32 as the firmware computes it (Util_Crc32)."""
        self.cpu.mem_write(SCRATCH, payload)
        return self.fw("Util_Crc32", SCRATCH, len(payload))

    def stored(self, offset=0, size=STORAGE_SIZE):
        return bytes(self.cpu.mem_read(self.storage + offset, size))

    def free_slots(self):
        return sum(self.get(self.mot(s, MOT_DEVICE)) == MOTOR_FREE for s in range(MOTOR_SLOTS))

    def motors(self, device):
        return {self.get(self.mot(s, MOT_MODULE)): s for s in range(MOTOR_SLOTS)
                if self.get(self.mot(s, MOT_DEVICE)) == device}

    def header(self, address):
        """(sequence, magic) of a slot."""
        sequence, magic = struct.unpack("<I8xI", bytes(self.cpu.mem_read(address, 16)))
        return sequence, magic

    def reboot(self):
        self.cpu.mem_write(self.storage, b"\xaa" * STORAGE_SIZE)
        self.fw("Config_Load")

    def test_blank_flash_loads_defaults_and_saves_cfg3(self):
        self.fw("Config_Load")
        self.assertEqual(self.stored(0, 1), b"\x01")
        self.assertTrue(all(self.get(self.cfg(i, CFG_DELETE)) == 1 for i in range(20)))
        self.assertEqual(self.free_slots(), MOTOR_SLOTS)
        self.assertEqual(self.header(SLOT_A), (1, CFG3))

    def test_saves_alternate_and_the_newest_slot_wins(self):
        self.fw("Config_Load")                                          # defaults -> A, sequence 1
        self.put(self.storage, 7)
        self.assertEqual(self.fw("Config_Save"), 0)
        self.assertEqual(self.header(SLOT_B), (2, CFG3))
        self.put(self.storage, 8)
        self.assertEqual(self.fw("Config_Save"), 0)
        self.assertEqual(self.header(SLOT_A), (3, CFG3))
        saved = self.stored()
        self.reboot()
        self.assertEqual(self.stored(), saved)

    def test_older_formats_are_not_read(self):
        # A CFG2 slot (885-byte payload) and a raw old block both load the defaults now.
        payload = bytes([9]) + b"\x00" * 884
        header = struct.pack("<IHHII", 5, len(payload), 0xFFFF, self.crc32(payload), CFG2)
        self.cpu.mem_write(SLOT_B, header + payload)
        raw = bytearray(b"\x00" * 885)
        raw[0], raw[835] = 9, 1                                         # net 9, reserved
        self.cpu.mem_write(SLOT_A, bytes(raw))
        self.fw("Config_Load")
        self.assertEqual(self.stored(0, 1), b"\x01")                     # default net, not 9
        self.assertEqual(self.header(SLOT_A), (1, CFG3))

    def test_cfg3_inconsistencies_are_repaired_on_load(self):
        self.fw("Config_Load")                                          # defaults
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
        self.assertEqual(bytes(self.cpu.mem_read(self.mot(self.motors(0)[0], MOT_NAME), 8)), b"Default\0")
        self.assertEqual(self.motors(5), {})
        self.assertEqual(self.free_slots(), MOTOR_SLOTS - 2)

    def test_interrupted_save_keeps_the_previous_slot(self):
        self.fw("Config_Load")                                          # defaults -> A
        self.put(self.storage, 7)
        self.assertEqual(self.fw("Config_Save"), 0)                     # -> B
        committed = self.stored()
        self.put(self.storage, 8)
        payload = self.stored()
        header = struct.pack("<IHHI", 3, STORAGE_SIZE, 0xFFFF, self.crc32(payload))  # magic never written
        self.cpu.mem_write(SLOT_A, header + b"\xff\xff\xff\xff" + payload)
        self.reboot()
        self.assertEqual(self.stored(), committed)


if __name__ == "__main__":
    unittest.main(verbosity=2)
