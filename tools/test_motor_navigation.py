"""Built-ARM checks of the flat motor list on the control screen (M11, M12, ..., M21, ...).
No board access."""
import unittest

import test_visibility as h
from test_visibility import CFG_DELETE, MOT_DEVICE, MOT_HIDE, MOT_LOCK, MOT_MODULE
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_PC, UC_ARM_REG_LR

# (device, module) -> pool slot. Device 1 is not paired; device 3 has module 0 hidden.
MOTORS = {(0, 0): 0, (0, 1): 20, (0, 2): 21, (2, 0): 2, (3, 0): 3, (3, 1): 22}
ORDER = [(0, 0), (0, 1), (0, 2), (2, 0), (3, 1)]   # visible motors in flat order


class MotorNavigationTests(h.FirmwareHarness):
    def setUp(self):
        super().setUp()
        self.queued = 0
        names = ["Main_SetBuzzerMode", "osMessageQueuePut", "ScrnDcs_ShowMotorLocked", "Schedule_CancelForMotor"]
        hooks = {h.symbol(n)[0] & ~1: n for n in names}

        def intercept(cpu, address, size, data):
            name = hooks.get(address)
            if name is None:
                return
            if name == "osMessageQueuePut":
                self.queued += 1
            cpu.reg_write(UC_ARM_REG_R0, 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE, intercept)
        for (device, module), slot in MOTORS.items():
            self.put(self.cfg(device, CFG_DELETE), 0)
            self.put(self.mot(slot, MOT_DEVICE), device)
            self.put(self.mot(slot, MOT_MODULE), module)
        self.put(self.mot(MOTORS[(3, 0)], MOT_HIDE), 1)

    def selected(self):
        return self.get(self.system + 60), self.get(self.system + 61)

    def walk(self, direction):
        visited = [self.selected()]
        for _ in range(len(ORDER) + 1):
            self.call("ScrnCommon_SelectAdjacentVisible", direction)
            visited.append(self.selected())
        return visited

    def test_right_steps_through_every_visible_motor_and_stops_at_the_end(self):
        self.select(0, 0)
        self.assertEqual(self.walk(1), ORDER + [ORDER[-1]] * 2)

    def test_left_walks_back_and_stops_at_the_start(self):
        self.select(3, 1)
        self.assertEqual(self.walk(-1), ORDER[::-1] + [ORDER[0]] * 2)

    def test_control_screen_buttons_move_between_modules(self):
        self.select(0, 0)
        self.call("screen_dcs.o:rightClicked")
        self.assertEqual(self.selected(), (0, 1))
        self.call("screen_dcs.o:leftClicked")
        self.assertEqual(self.selected(), (0, 0))

    def test_hidden_motor_is_replaced_by_the_next_visible_one(self):
        self.select(3, 0)                                    # hidden module 0 of device 3
        self.assertEqual(self.call("ScrnCommon_EnsureVisibleDevice"), 1)
        self.assertEqual(self.selected(), (3, 1))

    def test_last_motor_hidden_falls_back_to_the_previous_one(self):
        self.put(self.mot(MOTORS[(3, 1)], MOT_HIDE), 1)
        self.select(3, 1)
        self.assertEqual(self.call("ScrnCommon_EnsureVisibleDevice"), 1)
        self.assertEqual(self.selected(), (2, 0))

    def test_visible_selection_is_kept(self):
        self.select(0, 2)
        self.assertEqual(self.call("ScrnCommon_EnsureVisibleDevice"), 1)
        self.assertEqual(self.selected(), (0, 2))

    def test_device_with_one_visible_motor_is_still_listed(self):
        self.assertEqual(self.call("ScrnCommon_DeviceVisible", 3), 1)
        self.assertEqual(self.call("ScrnCommon_MotorVisible", 3, 0), 0)
        self.assertEqual(self.call("ScrnCommon_MotorVisible", 3, 1), 1)
        self.assertEqual(self.call("ScrnCommon_MotorVisible", 1, 0), 0)   # not paired

    def test_power_command_follows_the_selected_motor(self):
        self.select(0, 1)
        self.call("Main_SetMotorPower", 1)
        self.assertEqual(self.queued, 1)
        self.select(3, 0)                                    # hidden motor: nothing sent
        self.call("Main_SetMotorPower", 1)
        self.assertEqual(self.queued, 1)
        self.put(self.mot(MOTORS[(0, 2)], MOT_LOCK), 1)      # lock is per motor
        self.select(0, 2)
        self.call("Main_SetMotorPower", 1)
        self.assertEqual(self.queued, 1)
        self.select(0, 0)
        self.call("Main_SetMotorPower", 1)
        self.assertEqual(self.queued, 2)

    def test_no_visible_motor_clears_the_selection(self):
        for slot in MOTORS.values():
            self.put(self.mot(slot, MOT_HIDE), 1)
        self.select(2, 0)
        self.assertEqual(self.call("ScrnCommon_EnsureVisibleDevice"), 0)
        self.assertEqual(self.selected(), (0, 0))


if __name__ == "__main__":
    unittest.main(verbosity=2)
