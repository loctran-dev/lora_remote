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
        self.cpu.mem_write(self.storage + self.fields["sleepScanPeriod"], struct.pack("<I", 5))
        self.put(self.storage + self.fields["sleepEnabled"], 1)
        self.put(self.system + 68, 2)
        self.put(self.system + 62, 0)  # Simulate the completed settings save.

    def at(self, second):
        self.call("Link_ClockStep", second)
        self.call("Power_Tick")

    def value(self, offset):
        return int.from_bytes(self.cpu.mem_read(self.power + offset, 4), "little")

    def test_idle_enters_logical_sleep_without_user_activity(self):
        self.at(4)
        self.assertEqual(self.get(self.system + 68), 2)
        self.at(5)
        self.assertEqual(self.get(self.system + 68), 1)
        self.assertEqual(self.value(20), 10)

    def test_radio_activity_does_not_reset_user_idle_or_rtc_deadline(self):
        self.at(5)
        self.call("Power_RadioWakeISR")
        self.at(7)
        self.assertEqual(self.get(self.system + 68), 1)
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
        self.assertEqual(self.get(self.system + 68), 1)
        self.assertEqual(self.value(16), 0)

    def test_button_wakes_normal_mode(self):
        self.at(5)
        self.call("Power_ButtonWakeISR")
        self.at(7)
        self.assertEqual(self.get(self.system + 68), 2)
        self.assertEqual(self.value(16), 7)

    def test_confirmed_fault_stays_awake_until_user_acknowledges(self):
        self.at(5)
        self.call("Power_WakeForError")
        self.at(100)
        self.assertEqual(self.get(self.system + 68), 3)
        self.call("Power_UserActivity")
        self.assertEqual(self.get(self.system + 68), 2)

    def test_disabled_sleep_and_pending_flash_do_not_sleep(self):
        self.put(self.storage + self.fields["sleepEnabled"], 0)
        self.at(100)
        self.assertEqual(self.get(self.system + 68), 2)
        self.put(self.storage + self.fields["sleepEnabled"], 1)
        self.put(self.system + 62, 1)
        self.at(101)
        self.assertEqual(self.get(self.system + 68), 2)

    def test_active_usb_keeps_remote_awake(self):
        self.put(harness.symbol("hUsbDeviceFS")[0] + self.usb_state, 3)
        self.cpu.mem_write(harness.symbol("uwTick")[0], struct.pack("<I", 1000))
        self.call("Power_UsbSofISR")
        self.at(100)
        self.assertEqual(self.get(self.system + 68), 2)

    def test_timer_editor_keeps_remote_awake_without_usb(self):
        self.put(harness.symbol("inScheduleSelection")[0], 1)
        self.at(100)
        self.assertEqual(self.get(self.system + 68), 2)

    def test_pending_timer_command_keeps_remote_awake_without_usb(self):
        self.put(harness.symbol("scheduleRequest")[0] + 19, 1)
        self.at(100)
        self.assertEqual(self.get(self.system + 68), 2)

    def test_stop_is_blocked_during_confirmation_or_radio_work(self):
        self.put(self.system + 68, 1)
        self.put(harness.symbol("linkProbe")[0] + 22, 0)
        self.call("Power_TryStop")
        self.assertEqual(self.value(0), 0)
        self.put(harness.symbol("linkProbe")[0] + 22, 255)
        self.put(self.power + 29, 1)
        self.call("Power_TryStop")
        self.assertEqual(self.value(0), 0)

    def stop_entries(self, charging, full):
        """Run Power_TryStop() in an idle SLEEP with the charger outputs given; return STOP entries."""
        dwarf = subprocess.check_output([str(harness.FROMELF), "--text", "-g", str(harness.BUILD / "USB_Application.axf")],
                                        text=True)
        member = lambda name: int(re.findall(r"DW_AT_name " + name + r"\s+[^\n]*\n[^\n]*DW_AT_data_member_location"
                                             r"[^\n]*DW_OP_plus_uconst (\d+)", dwarf)[0])
        self.cpu.mem_map(0x40000000, 0x30000)           # TIM3, EXTI, RCC, PWR registers read as 0
        self.cpu.mem_map(0xE000E000, 0x1000)            # SysTick, SCB
        entries = []
        def stub(cpu, address, size, data):
            if address == harness.symbol("HAL_PWR_EnterSTOPMode")[0] & ~1:
                entries.append(1)
            cpu.reg_write(UC_ARM_REG_R0, 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
        for name in ["HAL_PWR_EnterSTOPMode", "power_sleep.o:restoreClock", "Main_SetRtcAlarmRelative",
                     "HAL_TIM_Base_Stop", "HAL_TIM_Base_Start", "HAL_ADC_Stop_DMA", "HAL_ADC_Start_DMA",
                     "HAL_SuspendTick", "HAL_ResumeTick", "vTaskSuspendAll", "xTaskResumeAll",
                     "osMessageQueueGetCount", "Power_ReadRtcSecond"]:
            address = harness.symbol(name)[0] & ~1
            self.cpu.hook_add(UC_HOOK_CODE, stub, begin=address, end=address)
        spi = dwarf[dwarf.index("DW_AT_name __SPI_HandleTypeDef"):]
        spi_state = int(re.search(r"DW_AT_name State\s+[^\n]*\n[^\n]*DW_OP_plus_uconst (\d+)", spi)[1])
        for name in ["hspi1", "hspi2"]:                  # SPI handles idle (HAL_SPI_STATE_READY = 1)
            self.put(harness.symbol(name)[0] + spi_state, 1)
        self.cpu.mem_write(harness.symbol("htim8")[0], struct.pack("<I", 0x40013400))  # TIM8 for the restart
        self.put(self.system + 68, 1)                   # runMode = SLEEP
        self.put(harness.symbol("linkProbe")[0] + 22, 255)
        self.put(harness.symbol("link_monitor.o:syncPending")[0], 0)  # boot status sync done
        self.put(harness.symbol("link_monitor.o:syncRequested")[0], 0)
        self.put(self.system + member("charging"), charging)
        self.put(self.system + member("batteryFull"), full)
        self.call("Power_TryStop", count=200000)
        return len(entries)

    def test_battery_only_enters_stop(self):
        self.assertEqual(self.stop_entries(charging=0, full=0), 1)

    def test_external_power_never_enters_stop(self):
        # KI-9: with USB/charger power the host may suspend the bus; STOP has no USB wake-up.
        self.assertEqual(self.stop_entries(charging=1, full=0), 0)

    def test_full_battery_on_charger_never_enters_stop(self):
        self.assertEqual(self.stop_entries(charging=0, full=1), 0)

if __name__ == "__main__":
    unittest.main(verbosity=2)
