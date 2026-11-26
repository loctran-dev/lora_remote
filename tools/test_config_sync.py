"""Built-ARM checks of the settings sync after pairing (KI-10): the remote sends each motor's
CMD_SET_CONFIG once the slave is heard on its new node. No board access."""
import struct
import unittest

import test_visibility as h
from test_visibility import CFG_DELETE
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP,
                              UC_ARM_REG_PC, UC_ARM_REG_LR)

CMD_SET_CONFIG = 0x08
SERIAL, NODE = 0xABCDEF01, 2
LAST_RX_TICK = 12      # DeviceInfo_t.lastRxTick


class ConfigSyncTests(h.FirmwareHarness):
    def setUp(self):
        super().setUp()
        self.put(self.system, 1)                                      # netId
        self.now = 1000
        self.packets, self.beeps = [], []
        names = ["osMessageQueuePut", "Main_SetBuzzerMode", "HAL_GetTick", "Link_RecordNodePacket"]
        hooks = {h.symbol(n)[0] & ~1: n for n in names}

        def intercept(cpu, address, size, data):
            name = hooks.get(address)
            if name is None:
                return
            result = 0
            if name == "osMessageQueuePut":
                self.packets.append(bytes(cpu.mem_read(cpu.reg_read(UC_ARM_REG_R1), 274)))
            elif name == "Main_SetBuzzerMode":
                self.beeps.append(cpu.reg_read(UC_ARM_REG_R0))
            elif name == "HAL_GetTick":
                result = self.now
            cpu.reg_write(UC_ARM_REG_R0, result)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE, intercept)

    # Helpers -------------------------------------------------------------
    def pair(self, modules=1):
        """Pair SERIAL on NODE through storePairedDevice (slot 0)."""
        self.cpu.mem_write(self.system + 52, struct.pack("<IBBB", SERIAL, 1, ord("M"), modules))
        self.put(self.system + 48, NODE)
        self.assertEqual(self.call("main.o:storePairedDevice") & 0xFF, 0)
        self.packets.clear()

    def heard(self, device=0):
        """The slave answered on its new node (status received)."""
        self.cpu.mem_write(self.dev(device, LAST_RX_TICK), struct.pack("<I", 1))

    def tick(self, now):
        self.now = now
        self.call("CmdHandler_ConfigSyncTick", now)

    def configs(self):
        """(node, module, threshold, delay, auto) of every queued CMD_SET_CONFIG."""
        return [(p[4],) + struct.unpack_from("<BHHH", p, 27) for p in self.packets if p[8] == CMD_SET_CONFIG]

    def reply(self, result=1):
        request = [p for p in self.packets if p[8] == CMD_SET_CONFIG][-1]
        packet = bytearray(274)
        packet[:9] = struct.pack("<HBBBBHB", 0xFF55, 9, 1, NODE, 2, struct.unpack_from("<H", request, 6)[0],
                                 CMD_SET_CONFIG)
        packet[10:14] = struct.pack("<I", 9 + 2 + 1)
        packet[18:27] = packet[:9]
        packet[27:29] = bytes([request[27], result])
        stack, stop = 0x2000E000, 0x0807FFF0
        self.cpu.mem_write(stack, bytes(packet[16:]))
        for reg, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3),
                              struct.unpack("<4I", packet[:16])):
            self.cpu.reg_write(reg, value)
        self.cpu.reg_write(UC_ARM_REG_SP, stack)
        self.cpu.reg_write(UC_ARM_REG_LR, stop | 1)
        self.cpu.emu_start(h.symbol("handleSetConfig")[0] | 1, stop, count=20000)
        self.assertEqual(self.cpu.reg_read(UC_ARM_REG_PC), stop)

    # Tests ---------------------------------------------------------------
    def test_nothing_is_sent_before_the_slave_is_heard_on_its_new_node(self):
        self.pair()
        self.tick(2000)
        self.assertEqual(self.configs(), [])

    def test_defaults_are_sent_to_the_new_node_without_a_beep(self):
        self.pair()
        self.heard()
        self.tick(2000)
        self.assertEqual(self.configs(), [(NODE, 0, 0, 0, 0)])     # threshold, delay, auto-status = 0
        self.reply(1)
        self.assertEqual(self.beeps, [])
        self.tick(3000)
        self.assertEqual(len(self.configs()), 1)                    # done, nothing more

    def test_every_motor_of_the_device_is_synced_in_turn(self):
        self.pair(modules=3)
        self.heard()
        for module in range(3):
            self.tick(2000 + module * 1000)
            self.assertEqual(self.configs()[-1][1], module)
            self.tick(2500 + module * 1000)                         # in flight: no second request
            self.assertEqual(len(self.configs()), module + 1)
            self.reply(1)
        self.tick(5000)
        self.assertEqual(len(self.configs()), 3)

    def test_rejected_sync_is_retried_later_then_given_up(self):
        self.pair()
        self.heard()
        for attempt in range(3):
            self.tick(2000 + attempt * 10000)
            self.assertEqual(len(self.configs()), attempt + 1)
            self.reply(0)                                           # e.g. motor ON
            self.tick(2000 + attempt * 10000 + 5000)                # before the retry pause ends
            self.assertEqual(len(self.configs()), attempt + 1)
        self.tick(100000)
        self.assertEqual(len(self.configs()), 3)
        self.assertEqual(self.beeps, [])

    def test_unanswered_sync_is_resent_after_the_timeout_and_pause(self):
        self.pair()
        self.heard()
        self.tick(2000)
        self.tick(6999)                                             # still waiting for the reply
        self.assertEqual(len(self.configs()), 1)
        self.tick(7000)                                             # timeout, then a 10 s pause
        self.tick(16999)
        self.assertEqual(len(self.configs()), 1)
        self.tick(17000)
        self.assertEqual(len(self.configs()), 2)

    def test_deleted_device_is_not_synced(self):
        self.pair()
        self.heard()
        self.put(self.cfg(0, CFG_DELETE), 1)
        self.tick(2000)
        self.assertEqual(self.configs(), [])

    def test_user_save_still_beeps(self):
        self.pair()
        self.select(0)
        self.call("CmdHandler_SendCurrentModuleConfig")
        self.reply(1)
        self.assertEqual(len(self.beeps), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
