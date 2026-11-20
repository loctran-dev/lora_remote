"""Built ARM logical sleep/wake policy tests; STOP clock/peripheral I/O needs hardware."""
import re
import struct
import subprocess
import unittest
import test_visibility as harness
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_PC, UC_ARM_REG_LR

class PowerPolicyTests(harness.FirmwareHarness):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        dwarf = subprocess.check_output([str(harness.FROMELF), "--text", "-g", str(harness.BUILD / "USB_Application.axf")], text=True)
        matches = re.findall(r"DW_AT_name dev_state\s+[^\n]*\n[^\n]*DW_AT_data_member_location[^\n]*DW_OP_plus_uconst (\d+)", dwarf)
        cls.usb_state = int(matches[0])

    def setUp(self):
        super().setUp()
        self.power = harness.symbol("powerSleep")[0]
        self.assertEqual(harness.symbol("powerSleep")[1], 36)
        self.gpio = []
        def pin(cpu, address, size, data):
            self.gpio.append(address)
            cpu.reg_write(UC_ARM_REG_R0, 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        for name in ["HAL_GPIO_Init", "HAL_GPIO_ReadPin", "HAL_GPIO_WritePin",
                     "LCD_Backlight_On", "LCD_Backlight_Off"]:
            address = harness.symbol(name)[0] & ~1
            self.cpu.hook_add(UC_HOOK_CODE, pin, begin=address, end=address)
        self.call("Link_Init", 0)
        self.call("Power_Init")
        self.call("Link_SetIdleSeconds", 5)
        self.cpu.mem_write(self.system + self.fields["tSleepUpdate"], struct.pack("<I", 5))
        self.put(self.system + self.fields["sleepMode"], 1)
        self.put(self.system + 92, 2)
        self.put(self.system + 82, 0)  # Simulate the completed settings save.

    def at(self, second):
        self.call("Link_ClockStep", second)
        self.call("Power_Tick")

    def value(self, offset):
        return int.from_bytes(self.cpu.mem_read(self.power + offset, 4), "little")

    def test_idle_enters_logical_sleep_without_user_activity(self):
        self.at(4)
        self.assertEqual(self.get(self.system + 92), 2)
        self.at(5)
        self.assertEqual(self.get(self.system + 92), 1)
        self.assertEqual(self.value(20), 10)

    def test_radio_activity_does_not_reset_user_idle_or_rtc_deadline(self):
        self.at(5)
        self.call("Power_RadioWakeISR")
        self.at(7)
        self.assertEqual(self.get(self.system + 92), 1)
        self.assertEqual(self.value(16), 0)
        self.assertEqual(self.value(20), 10)
        self.assertEqual(self.get(self.power + 29), 1)
        self.call("Power_RxProcessed")
        self.assertEqual(self.get(self.power + 29), 0)

    def test_rtc_wake_rearms_check_but_keeps_lcd_sleep_mode(self):
        self.at(5)
        self.call("Power_RtcWakeISR")
        self.at(10)
        self.assertEqual(self.value(20), 15)
        self.assertEqual(self.get(self.system + 92), 1)
        self.assertEqual(self.value(16), 0)

    def test_button_wakes_normal_mode(self):
        self.at(5)
        self.call("Power_ButtonWakeISR")
        self.at(7)
        self.assertEqual(self.get(self.system + 92), 2)
        self.assertEqual(self.value(16), 7)

    def test_confirmed_fault_stays_awake_until_user_acknowledges(self):
        self.at(5)
        self.call("Power_WakeForError")
        self.at(100)
        self.assertEqual(self.get(self.system + 92), 3)
        self.call("Power_UserActivity")
        self.assertEqual(self.get(self.system + 92), 2)

    def test_disabled_sleep_and_pending_flash_do_not_sleep(self):
        self.put(self.system + self.fields["sleepMode"], 0)
        self.at(100)
        self.assertEqual(self.get(self.system + 92), 2)
        self.put(self.system + self.fields["sleepMode"], 1)
        self.put(self.system + 82, 1)
        self.at(101)
        self.assertEqual(self.get(self.system + 92), 2)

    def test_active_usb_keeps_remote_awake(self):
        self.put(harness.symbol("hUsbDeviceFS")[0] + self.usb_state, 3)
        self.cpu.mem_write(harness.symbol("uwTick")[0], struct.pack("<I", 1000))
        self.call("Power_UsbSofISR")
        self.at(100)
        self.assertEqual(self.get(self.system + 92), 2)

    def test_timer_editor_keeps_remote_awake_without_usb(self):
        self.put(harness.symbol("inScheduleSelection")[0], 1)
        self.at(100)
        self.assertEqual(self.get(self.system + 92), 2)

    def test_pending_timer_command_keeps_remote_awake_without_usb(self):
        self.put(harness.symbol("scheduleRequest")[0] + 17, 1)
        self.at(100)
        self.assertEqual(self.get(self.system + 92), 2)

    def test_stop_is_blocked_during_confirmation_or_radio_work(self):
        self.put(self.system + 92, 1)
        self.put(harness.symbol("linkProbe")[0] + 22, 0)
        self.call("Power_TryStop")
        self.assertEqual(self.value(0), 0)
        self.put(harness.symbol("linkProbe")[0] + 22, 255)
        self.put(self.power + 29, 1)
        self.call("Power_TryStop")
        self.assertEqual(self.value(0), 0)

if __name__ == "__main__":
    unittest.main(verbosity=2)
