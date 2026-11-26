"""Built-ARM pairing checks in Unicorn: radio restore, slot reuse, motor pool and defaults. No board access."""
import struct
import unittest

import test_visibility as h
from test_visibility import (AUTO_STATUS, CFG_DELETE, CFG_MODULES, CFG_NODE, CFG_SERIAL, DEVICE_STRIDE,
                             MOT_DEVICE, MOT_HIDE, MOT_LOCK, MOT_MODULE, MOT_NAME, MOT_THRESHOLD, MOTOR_FREE,
                             MOTOR_SLOTS, MOTOR_STRIDE)
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_PC, UC_ARM_REG_LR

SERIAL_A, SERIAL_B = 0x11111111, 0xABCDEF01


class PairingTests(h.FirmwareHarness):
    def setUp(self):
        super().setUp()
        self.radio_inits = 0
        self.queued = 0
        names = ["osMutexAcquire", "osMutexRelease", "LoRa_init", "LoRa_startReceiving",
                 "osMessageQueuePut", "ScrnCommon_Switch", "Main_SetBuzzerMode", "xTaskGetTickCount"]
        hooks = {h.symbol(n)[0] & ~1: n for n in names}

        def intercept(cpu, address, size, data):
            name = hooks.get(address)
            if name is None:
                return
            if name == "LoRa_init":
                self.radio_inits += 1
            if name == "osMessageQueuePut":
                self.queued += 1
            cpu.reg_write(UC_ARM_REG_R0, 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE, intercept)

    def u32(self, address, value=None):
        if value is None:
            return struct.unpack("<I", bytes(self.cpu.mem_read(address, 4)))[0]
        self.cpu.mem_write(address, struct.pack("<I", value))

    def text(self, address):
        return bytes(self.cpu.mem_read(address, 20)).split(b"\0")[0].decode()

    def free_slots(self):
        return sum(self.get(self.mot(s, MOT_DEVICE)) == MOTOR_FREE for s in range(MOTOR_SLOTS))

    def motors(self, device):
        """{module: pool slot} of a device."""
        return {self.get(self.mot(slot, MOT_MODULE)): slot for slot in range(MOTOR_SLOTS)
                if self.get(self.mot(slot, MOT_DEVICE)) == device}

    def paired(self, device, serial, node, name=b"Pump", lock=1, hide=1):
        """A paired device with one motor (pool slot = device) with non-default settings."""
        self.add(device, hidden=bool(hide))
        self.put(self.cfg(device, CFG_NODE), node)
        self.u32(self.cfg(device, CFG_SERIAL), serial)
        self.put(self.cfg(device, CFG_MODULES), 1)
        self.cpu.mem_write(self.mot(device, MOT_NAME), name + b"\0")
        self.cpu.mem_write(self.mot(device, MOT_THRESHOLD), struct.pack("<fI", 2.5, 30))
        self.put(self.mot(device, MOT_LOCK), lock)
        self.cpu.mem_write(self.storage + AUTO_STATUS + device * 2, struct.pack("<H", 60))
        self.put(self.dev(device, h.DEV_CONNECTION_ERROR), 1)

    def found(self, serial, node, modules=1, device_type=1):
        """Factory info from CMD_GET_FACTORY_INFO and the node id sent with CONFIG_CONNECTION."""
        self.u32(self.system + 52, serial)
        self.put(self.system + 56, device_type)
        self.put(self.system + 57, ord("M"))
        self.put(self.system + 58, modules)
        self.put(self.system + 48, node)

    def fill_pool(self, owner=19):
        """Occupy every pool slot with motors of another device."""
        self.add(owner)
        for slot in range(MOTOR_SLOTS):
            self.put(self.mot(slot, MOT_DEVICE), owner)
            self.put(self.mot(slot, MOT_MODULE), slot)  # distinct keys; modules >= 6 never looked up

    def assert_defaults(self, device, serial, node, modules=1):
        self.assertEqual(self.get(self.dev(device, 0)), device)                    # index
        self.assertEqual(self.get(self.dev(device, h.DEV_CONNECTION_ERROR)), 0)    # runtime state cleared
        self.assertEqual(self.get(self.cfg(device, CFG_NODE)), node)
        self.assertEqual(self.u32(self.cfg(device, CFG_SERIAL)), serial)
        self.assertEqual(self.get(self.cfg(device, CFG_DELETE)), 0)
        slots = self.motors(device)
        self.assertEqual(sorted(slots), list(range(modules)))
        for slot in slots.values():
            self.assertEqual(self.text(self.mot(slot, MOT_NAME)), "Default")
            self.assertEqual(bytes(self.cpu.mem_read(self.mot(slot, MOT_THRESHOLD), MOTOR_STRIDE - MOT_THRESHOLD)),
                             bytes(MOTOR_STRIDE - MOT_THRESHOLD))             # alarm, lock, hide
        self.assertEqual(bytes(self.cpu.mem_read(self.storage + AUTO_STATUS + device * 2, 2)), bytes(2))
        self.assertEqual(self.get(self.system + 62), 1)                       # configSavePending

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
        self.cpu.mem_write(self.mot(1, MOT_NAME), b"Leftover\0")             # stale data in a free pool slot
        self.put(self.mot(1, MOT_LOCK), 1)
        self.found(SERIAL_B, 2)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 1)
        self.assert_defaults(1, SERIAL_B, 2)
        self.assertEqual(self.text(self.mot(0, MOT_NAME)), "Pump")            # other device untouched

    def test_device_with_several_modules_gets_one_motor_slot_each(self):
        self.found(SERIAL_B, 2, modules=3)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 0)
        self.assert_defaults(0, SERIAL_B, 2, modules=3)

    def test_module_count_is_limited_to_one_to_six(self):
        self.assertEqual([self.call("Config_ModuleCount", n) & 0xFF for n in (0, 1, 6, 7, 255)], [1, 1, 6, 6, 6])

    def test_repairing_reuses_slot_and_resets_its_settings(self):
        self.paired(3, SERIAL_B, 7)
        self.found(SERIAL_B, 7)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 3)
        self.assert_defaults(3, SERIAL_B, 7)
        self.assertEqual(self.get(self.cfg(0, CFG_DELETE)), 1)                # no extra device slot taken

    def test_repairing_with_fewer_modules_releases_the_rest(self):
        self.found(SERIAL_B, 7, modules=4)
        self.call("main.o:storePairedDevice")
        free = self.free_slots()
        self.found(SERIAL_B, 7, modules=2)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 0)
        self.assert_defaults(0, SERIAL_B, 7, modules=2)
        self.assertEqual(self.free_slots(), free + 2)

    def test_connect_resends_node_id_of_paired_device(self):
        self.paired(3, SERIAL_B, 7)
        self.found(SERIAL_B, 0)
        self.assertEqual(self.call("Main_ConnectToDevice") & 0xFF, 1)
        self.assertEqual(self.get(self.system + 48), 7)
        self.assertEqual(self.queued, 1)

    def test_deleted_slot_with_same_serial_is_a_new_pairing(self):
        self.paired(3, SERIAL_B, 7)
        self.put(self.cfg(3, CFG_DELETE), 1)
        self.found(SERIAL_B, 0)
        self.call("Main_ConnectToDevice")
        self.assertEqual(self.get(self.system + 48), 1)                       # first unused node id
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 0)

    def test_full_device_table_is_refused_before_contacting_the_device(self):
        for device in range(20):
            self.paired(device, SERIAL_A + device, device + 1)
        self.found(SERIAL_B, 30)
        self.assertEqual(self.call("Main_ConnectToDevice") & 0xFF, 0)
        self.assertEqual(self.queued, 0)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 0xFF)
        self.assertEqual(self.get(self.system + 62), 0)

    def test_full_motor_pool_is_refused_before_contacting_the_device(self):
        self.fill_pool()
        self.found(SERIAL_B, 2)
        self.assertEqual(self.call("Main_ConnectToDevice") & 0xFF, 0)
        self.assertEqual(self.queued, 0)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 0xFF)
        self.assertEqual(self.get(self.cfg(0, CFG_DELETE)), 1)               # nothing stored

    def test_repairing_may_reuse_its_own_slots_when_the_pool_is_full(self):
        self.fill_pool()
        self.put(self.cfg(3, CFG_DELETE), 0)                                 # paired, without add()'s slot 3
        self.u32(self.cfg(3, CFG_SERIAL), SERIAL_B)
        self.put(self.mot(5, MOT_DEVICE), 3)                                  # device 3 holds one slot
        self.put(self.mot(5, MOT_MODULE), 0)
        self.found(SERIAL_B, 7)
        self.assertEqual(self.call("Main_ConnectToDevice") & 0xFF, 1)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 3)
        self.assertEqual(self.motors(3), {0: 5})

    def test_device_hidden_only_when_every_motor_is_hidden(self):
        self.found(SERIAL_B, 2, modules=2)
        self.call("main.o:storePairedDevice")
        slots = self.motors(0)
        self.assertEqual(self.call("Config_DeviceHidden", 0) & 0xFF, 0)
        self.put(self.mot(slots[0], MOT_HIDE), 1)
        self.assertEqual(self.call("Config_DeviceHidden", 0) & 0xFF, 0)       # module 1 still visible
        self.call("Config_SetDeviceHidden", 0, 1)
        self.assertEqual(self.call("Config_DeviceHidden", 0) & 0xFF, 1)
        self.assertEqual([self.get(self.mot(s, MOT_HIDE)) for s in slots.values()], [1, 1])
        self.assertEqual(self.call("Config_DeviceHidden", 7) & 0xFF, 1)       # no motors at all

    def test_free_motors_releases_only_that_device(self):
        self.paired(0, SERIAL_A, 1)
        self.paired(1, SERIAL_B, 2)
        self.call("Config_FreeMotors", 0)
        self.assertEqual(self.motors(0), {})
        self.assertEqual(self.motors(1), {0: 1})
        self.assertEqual(self.get(self.mot(0, MOT_DEVICE)), MOTOR_FREE)

    def test_pairing_keeps_link_settings_valid_across_reboot(self):
        # autoStatusPeriod is covered by the link-settings checksum; a raw write made
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

    def test_pairing_clears_runtime_state_left_in_reused_pool_slots(self):
        # motorsData[i] follows motors[i]; a freed slot still holds the old motor's state.
        self.cpu.mem_write(self.state_of(0), b"\x01" * h.MODULE_STRIDE)
        self.found(SERIAL_B, 2)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 0)
        self.assertEqual(self.motors(0), {0: 0})
        self.assertEqual(bytes(self.cpu.mem_read(self.state_of(0), h.MODULE_STRIDE)), bytes(h.MODULE_STRIDE))

    def test_device_buffer_holds_runtime_state_only(self):
        self.assertEqual(h.symbol("devicesInfo")[1], 20 * DEVICE_STRIDE)


if __name__ == "__main__":
    unittest.main(verbosity=2)
