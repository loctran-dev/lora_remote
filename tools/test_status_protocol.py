"""Built-ARM checks of CMD_GET_STATUS for every module (LORA_MODULE_ALL), per-module status
and alarms, and per-motor schedule tracking (KI-1, KI-2). No board access."""
import struct
import unittest

import test_visibility as h
from test_visibility import CFG_NODE, MOT_DEVICE, MOT_MODULE
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP,
                              UC_ARM_REG_PC, UC_ARM_REG_LR)

ALL = 0xFF
CMD_GET_STATUS, CMD_SET_SCHEDULE, CMD_ALARM = 0x01, 0x03, 0x07
REQUEST, RESPONSE = 1, 2
NODE = 1
SLOTS = {0: 0, 1: 20, 2: 21}            # device 0 drives modules 0..2 (pool slots)
PWR, CURRENT, OFF_TIME = 8, 12, 24      # MotorData_t offsets


def record(module, power=1, currents=(0, 0, 0), deadline=0, result=1):
    return struct.pack("<BBB3HI", module, result, power, *currents, deadline)


def all_status(records, local_time=1000):
    return struct.pack("<BIB", ALL, local_time, len(records)) + b"".join(records)


def single_status(module, power=1, currents=(0, 0, 0), deadline=0, result=1, local_time=1000):
    return struct.pack("<BBB3HII", module, result, power, *currents, local_time, deadline)


class StatusProtocolTests(h.FirmwareHarness):
    def setUp(self):
        super().setUp()
        self.add(0)
        self.select(0)
        self.put(self.system, 1)                                       # netId
        self.put(self.cfg(0, CFG_NODE), NODE)
        for module, slot in SLOTS.items():
            self.put(self.mot(slot, MOT_DEVICE), 0)
            self.put(self.mot(slot, MOT_MODULE), module)
        self.packets = []
        names = ["osMessageQueuePut", "Main_SetBuzzerMode", "LoRa_getRSSI", "xTaskGetTickCount",
                 "Power_WakeForError", "Link_RecordPacket", "Link_RecordNodePacket", "Link_RecordStatusReply"]
        hooks = {h.symbol(n)[0] & ~1: n for n in names}

        def intercept(cpu, address, size, data):
            name = hooks.get(address)
            if name is None:
                return
            if name == "osMessageQueuePut":
                self.packets.append(bytes(cpu.mem_read(cpu.reg_read(UC_ARM_REG_R1), 274)))
            cpu.reg_write(UC_ARM_REG_R0, 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE, intercept)

    def deliver(self, handler, code, kind, payload, transaction=7):
        """Pass a received LoraPackage_t (by value) to a command handler."""
        packet = bytearray(274)
        packet[:9] = struct.pack("<HBBBBHB", 0xFF55, 9 + len(payload) - 1, 1, NODE, kind, transaction, code)
        packet[10:14] = struct.pack("<I", 9 + len(payload) + 1)
        packet[18:27] = packet[:9]
        packet[27:27 + len(payload)] = payload
        stack, stop = 0x2000E000, 0x0807FFF0
        self.cpu.mem_write(stack, bytes(packet[16:]))
        for reg, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3),
                              struct.unpack("<4I", packet[:16])):
            self.cpu.reg_write(reg, value)
        self.cpu.reg_write(UC_ARM_REG_SP, stack)
        self.cpu.reg_write(UC_ARM_REG_LR, stop | 1)
        self.cpu.emu_start(h.symbol(handler)[0] | 1, stop, count=50000)
        self.assertEqual(self.cpu.reg_read(UC_ARM_REG_PC), stop)

    def status(self, payload, transaction=7):
        self.deliver("handleGetStatus", CMD_GET_STATUS, RESPONSE, payload, transaction)

    def power(self, module):
        return self.get(self.state_of(SLOTS[module], PWR))

    def currents(self, module):
        return struct.unpack("<3f", bytes(self.cpu.mem_read(self.state_of(SLOTS[module], CURRENT), 12)))

    def off_time(self, module):
        return struct.unpack("<I", bytes(self.cpu.mem_read(self.state_of(SLOTS[module], OFF_TIME), 4)))[0]

    # Status --------------------------------------------------------------
    def test_all_status_updates_every_reported_motor(self):
        self.status(all_status([record(0, 1, (100, 0, 0)), record(1, 2, (0, 250, 0), deadline=5000),
                                record(2, 1, (0, 0, 75))], local_time=1234))
        self.assertEqual([self.power(m) for m in range(3)], [1, 2, 1])
        self.assertAlmostEqual(self.currents(0)[0], 1.0, places=3)
        self.assertAlmostEqual(self.currents(1)[1], 2.5, places=3)
        self.assertAlmostEqual(self.currents(2)[2], 0.75, places=3)
        self.assertEqual(self.off_time(1), 5000)
        self.assertEqual(struct.unpack("<I", bytes(self.cpu.mem_read(self.dev(0, 4), 4)))[0], 1234)  # devTime

    def test_device_may_report_fewer_motors_than_slots(self):
        self.status(all_status([record(0, 1)]))
        self.assertEqual([self.power(m) for m in range(3)], [1, 0, 0])

    def test_records_without_a_slot_or_invalid_are_skipped(self):
        self.status(all_status([record(4, 1), record(1, 1, result=0), record(2, 2)]))
        self.assertEqual([self.power(m) for m in range(3)], [0, 0, 2])
        self.assertEqual(self.get(self.state_of(39, PWR)), 0)        # nothing written outside the slots

    def test_single_status_of_another_module_is_accepted(self):
        self.status(single_status(1, power=2, currents=(0, 30, 0)))  # KI-1
        self.assertEqual([self.power(m) for m in range(3)], [0, 2, 0])
        self.assertAlmostEqual(self.currents(1)[1], 0.3, places=3)

    def test_malformed_all_status_is_ignored(self):
        for payload in [struct.pack("<BIB", ALL, 1000, 0),                       # no records
                        all_status([record(0)])[:-1],                           # short record
                        all_status([record(0)]) + b"\x00",                      # trailing byte
                        struct.pack("<BIB", ALL, 1000, 7) + record(0) * 7]:    # too many records
            self.status(payload)
        self.assertEqual([self.power(m) for m in range(3)], [0, 0, 0])

    # Alarms --------------------------------------------------------------
    def test_alarm_of_another_module_reaches_that_motor(self):
        self.deliver("handleAlarmMessage", CMD_ALARM, REQUEST, struct.pack("<BBI", 2, 1, 777))  # KI-1
        self.assertEqual(self.get(self.state_of(SLOTS[2], 0)), 1)     # alarmReported
        self.assertEqual(self.get(self.state_of(SLOTS[0], 0)), 0)

    # Schedules (KI-2) -----------------------------------------------------
    def start_timer(self, module, seconds=60, now=1000):
        self.cpu.mem_write(self.system + 28, struct.pack("<I", now))  # currentTime
        self.select(0, module)
        self.call("Main_SetScheduleTimeToCurrentModule", seconds)
        return now + seconds

    def schedule_state(self, module):
        return self.call("Schedule_GetStateForMotor", NODE, module) & 0xFF

    def test_timer_state_belongs_to_one_motor(self):
        self.start_timer(1)
        self.assertEqual(self.schedule_state(1), 1)                   # waiting
        self.assertEqual(self.schedule_state(0), 0)                   # other motor: idle

    def test_status_of_another_module_does_not_confirm_the_timer(self):
        deadline = self.start_timer(1)
        self.status(all_status([record(0, 1, deadline=deadline)]))
        self.assertEqual(self.schedule_state(1), 1)
        self.status(all_status([record(0, 1), record(1, 1, deadline=deadline)]))
        self.assertEqual(self.schedule_state(1), 2)                   # confirmed

    def test_switching_another_motor_keeps_the_timer(self):
        self.start_timer(1)
        self.call("Schedule_CancelForMotor", NODE, 0)
        self.assertEqual(self.schedule_state(1), 1)
        self.call("Schedule_CancelForMotor", NODE, 1)
        self.assertEqual(self.schedule_state(1), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
