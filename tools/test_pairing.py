"""Built-ARM pairing checks in Unicorn: radio restore, slot reuse and default settings. No board access."""
import struct
import unittest

import test_visibility as h
from test_visibility import (CFG_DELAY, CFG_DELETE, CFG_HIDE, CFG_LOCK, CFG_NAME, CFG_NODE, CFG_SERIAL,
                             CFG_STRIDE, CFG_THRESHOLD, DEVICE_STRIDE)
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_PC, UC_ARM_REG_LR

SERIAL_A, SERIAL_B = 0x11111111, 0xABCDEF01
AUTO_STATUS = 844         # storageData.autoStatusSeconds[]


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

    def u32(self, address, value=None):
        if value is None:
            return struct.unpack("<I", bytes(self.cpu.mem_read(address, 4)))[0]
        self.cpu.mem_write(address, struct.pack("<I", value))

    def text(self, address):
        return bytes(self.cpu.mem_read(address, 20)).split(b"\0")[0].decode()

    def paired(self, slot, serial, node, name=b"Pump", lock=1, hide=1):
        """A used slot with non-default settings and some runtime state."""
        self.add(slot, hidden=bool(hide))
        self.put(self.cfg(slot, CFG_NODE), node)
        self.u32(self.cfg(slot, CFG_SERIAL), serial)
        self.cpu.mem_write(self.cfg(slot, CFG_NAME), name + b"\0")
        self.cpu.mem_write(self.cfg(slot, CFG_THRESHOLD), struct.pack("<fI", 2.5, 30))
        self.put(self.cfg(slot, CFG_LOCK), lock)
        self.cpu.mem_write(self.storage + AUTO_STATUS + slot * 2, struct.pack("<H", 60))
        self.put(self.dev(slot, h.DEV_CONNECTION_ERROR), 1)

    def found(self, serial, node, device_type=1):
        """Factory info from CMD_GET_FACTORY_INFO and the node id sent with CONFIG_CONNECTION."""
        self.u32(self.system + 52, serial)
        self.put(self.system + 56, device_type)
        self.put(self.system + 57, ord("M"))
        self.put(self.system + 58, 1)
        self.put(self.system + 48, node)

    def assert_defaults(self, slot, serial, node):
        self.assertEqual(self.get(self.dev(slot, 0)), slot)                   # deviceIndex
        self.assertEqual(self.get(self.dev(slot, h.DEV_CONNECTION_ERROR)), 0)  # runtime state cleared
        self.assertEqual(self.get(self.cfg(slot, CFG_NODE)), node)
        self.assertEqual(self.u32(self.cfg(slot, CFG_SERIAL)), serial)
        self.assertEqual(self.text(self.cfg(slot, CFG_NAME)), "Default")
        self.assertEqual(bytes(self.cpu.mem_read(self.cfg(slot, CFG_THRESHOLD), CFG_STRIDE - CFG_THRESHOLD)),
                         bytes(CFG_STRIDE - CFG_THRESHOLD))                   # alarm, lock, hide, delete
        self.assertEqual(bytes(self.cpu.mem_read(self.storage + AUTO_STATUS + slot * 2, 2)), bytes(2))
        self.assertEqual(self.get(self.system + 62), 1)                       # isNeedSaveConfig

    def test_leaving_pairing_restores_stored_radio_settings(self):
        self.put(self.storage, 1)
        self.u32(self.storage + 1, 868)
        self.put(self.storage + 5, 9)
        self.put(self.system, 0)                                              # pairing network
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
        self.cpu.mem_write(self.cfg(1, CFG_NAME), b"Leftover\0")             # stale data in a free slot
        self.put(self.cfg(1, CFG_LOCK), 1)
        self.found(SERIAL_B, 2)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 1)
        self.assert_defaults(1, SERIAL_B, 2)
        self.assertEqual(self.text(self.cfg(0, CFG_NAME)), "Pump")            # other slot untouched

    def test_repairing_reuses_slot_and_resets_its_settings(self):
        self.paired(3, SERIAL_B, 7)
        self.found(SERIAL_B, 7)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 3)
        self.assert_defaults(3, SERIAL_B, 7)
        self.assertEqual(self.get(self.cfg(0, CFG_DELETE)), 1)                # no extra slot taken

    def test_connect_resends_node_id_of_paired_device(self):
        self.paired(3, SERIAL_B, 7)
        self.found(SERIAL_B, 0)
        self.call("Main_ConnectToDevice")
        self.assertEqual(self.get(self.system + 48), 7)

    def test_deleted_slot_with_same_serial_is_a_new_pairing(self):
        self.paired(3, SERIAL_B, 7)
        self.put(self.cfg(3, CFG_DELETE), 1)
        self.found(SERIAL_B, 0)
        self.call("Main_ConnectToDevice")
        self.assertEqual(self.get(self.system + 48), 1)                       # first unused node id
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 0)

    def test_full_table_is_rejected(self):
        for slot in range(20):
            self.paired(slot, SERIAL_A + slot, slot + 1)
        self.found(SERIAL_B, 30)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 0xFF)
        self.assertEqual(self.get(self.system + 62), 0)

    def test_pairing_keeps_link_settings_valid_across_reboot(self):
        # autoStatusSeconds is covered by the link-settings checksum; a raw write made
        # Link_Init() reset every link setting to its default on the next boot.
        self.call("Link_SetIdleSeconds", 45)
        self.call("Link_SetTimeoutSeconds", 180)
        self.call("Link_SetAutoStatus", 0, 2)
        self.paired(3, SERIAL_B, 7)
        self.call("Link_SetAutoStatus", 3, 60)
        self.found(SERIAL_B, 7)
        self.call("main.o:storePairedDevice")
        self.call("Link_Init", 0)
        self.assertEqual(self.call("Link_GetIdleSeconds") & 0xFFFF, 45)
        self.assertEqual(self.call("Link_GetAutoStatus", 0) & 0xFFFF, 2)
        self.assertEqual(self.call("Link_GetAutoStatus", 3) & 0xFFFF, 0)

    def test_device_buffer_holds_runtime_state_only(self):
        self.assertEqual(h.symbol("deviceBuff")[1], 20 * DEVICE_STRIDE)


if __name__ == "__main__":
    unittest.main(verbosity=2)
