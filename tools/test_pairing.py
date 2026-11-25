"""Built-ARM pairing checks in Unicorn: radio restore, slot reuse and default settings. No board access."""
import struct
import unittest

import test_visibility as h
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_PC, UC_ARM_REG_LR

STRIDE = 396              # sizeof(DeviceInfo_t)
STORE_STRIDE = 41         # sizeof(DeviceStorageData_t), packed
SERIAL_A, SERIAL_B = 0x11111111, 0xABCDEF01


class PairingTests(h.FirmwareHarness):
    def setUp(self):
        super().setUp()
        self.radio_inits = 0
        names = ["osMutexAcquire", "osMutexRelease", "LoRa_init", "LoRa_startReceiving",
                 "osMessageQueuePut", "ScrnCommon_Switch", "Main_SetBuzzerMode", "xTaskGetTickCount"]
        hooks = {h.symbol(n)[0] & ~1: n for n in names}

        def intercept(cpu, address, size, data):
            name = hooks.get(address)
            if name is None:
                return
            if name == "LoRa_init":
                self.radio_inits += 1
            cpu.reg_write(UC_ARM_REG_R0, 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE, intercept)

    # Helpers addressing DeviceInfo_t / SystemInfo_t / storage fields (offsets from AXF DWARF).
    def dev(self, slot, offset):
        return self.device + slot * STRIDE + offset

    def cfg(self, slot, offset):
        return self.storage + 15 + slot * STORE_STRIDE + offset

    def u32(self, address, value=None):
        if value is None:
            return struct.unpack("<I", bytes(self.cpu.mem_read(address, 4)))[0]
        self.cpu.mem_write(address, struct.pack("<I", value))

    def text(self, address):
        return bytes(self.cpu.mem_read(address, 20)).split(b"\0")[0].decode()

    def paired(self, slot, serial, node, name=b"Pump", lock=1, hide=1):
        """A used slot with non-default settings in RAM and in storage."""
        self.add(slot, hidden=bool(hide))
        self.put(self.dev(slot, 1), node)
        self.u32(self.dev(slot, 4), serial)
        self.cpu.mem_write(self.dev(slot, 64), name + b"\0")
        self.cpu.mem_write(self.dev(slot, 84), struct.pack("<f", 2.5))
        self.u32(self.dev(slot, 88), 30)
        self.put(self.dev(slot, 92), lock)
        self.cpu.mem_write(self.cfg(slot, 10), name + b"\0")
        self.put(self.cfg(slot, 38), lock)
        self.put(self.cfg(slot, 39), hide)
        self.cpu.mem_write(self.storage + 844 + slot * 2, struct.pack("<H", 60))

    def found(self, serial, node, device_type=1):
        """Factory info from CMD_GET_FACTORY_INFO and the node id sent with CONFIG_CONNECTION."""
        self.u32(self.system + 52, serial)
        self.put(self.system + 56, device_type)
        self.put(self.system + 57, ord("M"))
        self.put(self.system + 58, 1)
        self.put(self.system + 48, node)

    def assert_defaults(self, slot, serial, node):
        self.assertEqual(self.get(self.dev(slot, 0)), slot)          # deviceIndex
        self.assertEqual(self.get(self.dev(slot, 1)), node)
        self.assertEqual(self.u32(self.dev(slot, 4)), serial)
        self.assertEqual(self.get(self.dev(slot, 20)), 0)            # isDelete
        self.assertEqual(self.text(self.dev(slot, 64)), "Default")
        self.assertEqual(self.u32(self.dev(slot, 84)), 0)            # alarmThreshold 0.0
        self.assertEqual(self.u32(self.dev(slot, 88)), 0)            # alarmDelayTime
        self.assertEqual(self.get(self.dev(slot, 92)), 0)            # isLock
        self.assertEqual(self.get(self.dev(slot, 93)), 0)            # isHide
        self.assertEqual(self.get(self.cfg(slot, 0)), node)
        self.assertEqual(self.u32(self.cfg(slot, 2)), serial)
        self.assertEqual(self.text(self.cfg(slot, 10)), "")          # shown as "Default" after reset
        self.assertEqual(bytes(self.cpu.mem_read(self.cfg(slot, 30), 11)), bytes(11))
        self.assertEqual(bytes(self.cpu.mem_read(self.storage + 844 + slot * 2, 2)), bytes(2))
        self.assertEqual(self.get(self.system + 62), 1)              # isNeedSaveConfig

    def test_leaving_pairing_restores_stored_radio_settings(self):
        self.put(self.storage, 1)
        self.u32(self.storage + 1, 868)
        self.put(self.storage + 5, 9)
        self.put(self.system, 0)                                     # pairing network
        self.put(self.system + 34, 1)
        self.put(self.system + 40, 1)
        lora = h.symbol("sLoRaCtrl")[0]
        self.u32(lora + 32, 433)
        self.call("screen_dps.o:rightPressed")
        self.assertEqual(self.get(self.system), 1)
        self.assertEqual(self.u32(lora + 32), 868)
        self.assertEqual(self.get(lora + 36), 9)
        self.assertEqual((self.get(self.system + 34), self.get(self.system + 40)), (0, 0))
        self.assertEqual(self.radio_inits, 1)

    def test_scan_clears_previous_factory_info(self):
        self.cpu.mem_write(self.system + 52, b"\x26" * 8)
        self.call("Main_ScanNewDevice")
        self.assertEqual(bytes(self.cpu.mem_read(self.system + 52, 8)), bytes(8))
        self.assertEqual(self.get(self.system), 0)

    def test_type_letters(self):
        self.assertEqual([chr(self.call("main.o:deviceTypeChar", t)) for t in range(5)],
                         ["x", "M", "P", "E", "x"])

    def test_new_device_takes_first_free_slot_with_defaults(self):
        self.paired(0, SERIAL_A, 1)
        self.cpu.mem_write(self.dev(1, 64), b"Leftover\0")           # stale data in a free slot
        self.put(self.dev(1, 92), 1)
        self.found(SERIAL_B, 2)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 1)
        self.assert_defaults(1, SERIAL_B, 2)
        self.assertEqual(self.text(self.dev(0, 64)), "Pump")         # other slot untouched

    def test_repairing_reuses_slot_and_resets_its_settings(self):
        self.paired(3, SERIAL_B, 7)
        self.found(SERIAL_B, 7)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 3)
        self.assert_defaults(3, SERIAL_B, 7)
        self.assertEqual(self.get(self.dev(0, 20)), 1)               # no extra slot taken

    def test_connect_resends_node_id_of_paired_device(self):
        self.paired(3, SERIAL_B, 7)
        self.found(SERIAL_B, 0)
        self.call("Main_ConnectToDevice")
        self.assertEqual(self.get(self.system + 48), 7)

    def test_deleted_slot_with_same_serial_is_a_new_pairing(self):
        self.paired(3, SERIAL_B, 7)
        self.put(self.dev(3, 20), 1)                                 # deleted
        self.found(SERIAL_B, 0)
        self.call("Main_ConnectToDevice")
        self.assertEqual(self.get(self.system + 48), 1)              # first unused node id
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 0)

    def test_full_table_is_rejected(self):
        for slot in range(20):
            self.paired(slot, SERIAL_A + slot, slot + 1)
        self.found(SERIAL_B, 30)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 0xFF)
        self.assertEqual(self.get(self.system + 62), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
