"""Built ARM fault injection: stuck DMA, failed SPI and finite LCD recovery."""
import struct
import re
import unittest
import test_visibility as harness
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                              UC_ARM_REG_R3, UC_ARM_REG_PC, UC_ARM_REG_LR)


class IoFaultTests(harness.FirmwareHarness):
    def setUp(self):
        super().setUp()
        self.cpu.mem_map(0x40000000, 0x30000)
        self.tick = 0
        self.tick_step = 1
        self.dma_state = self.spi_state = 1  # READY
        self.dma_error = self.spi_error = 0
        self.tx_result = self.rx_result = self.dma_init_result = self.spi_init_result = 0
        self.dma_start_result = 0
        self.io_calls = []
        names = ["HAL_GetTick", "HAL_SPI_Transmit", "HAL_SPI_Receive",
                 "HAL_SPI_Transmit_DMA", "HAL_DMA_GetState", "HAL_SPI_GetState",
                 "HAL_DMA_GetError", "HAL_SPI_GetError", "HAL_SPI_Abort",
                 "HAL_DMA_Abort", "HAL_DMA_Init", "HAL_SPI_Init", "HAL_GPIO_WritePin"]
        hooks = {harness.symbol(name)[0] & ~1: name for name in names}

        def intercept(cpu, address, size, data):
            if address not in hooks:
                return
            name = hooks[address]
            args = [cpu.reg_read(reg) for reg in (UC_ARM_REG_R0, UC_ARM_REG_R1,
                                                  UC_ARM_REG_R2, UC_ARM_REG_R3)]
            self.io_calls.append((name, args))
            result = 0
            if name == "HAL_GetTick":
                result = self.tick
                self.tick = (self.tick + self.tick_step) & 0xFFFFFFFF
            elif name == "HAL_DMA_GetState": result = self.dma_state
            elif name == "HAL_SPI_GetState": result = self.spi_state
            elif name == "HAL_DMA_GetError": result = self.dma_error
            elif name == "HAL_SPI_GetError": result = self.spi_error
            elif name == "HAL_SPI_Transmit": result = self.tx_result
            elif name == "HAL_SPI_Receive": result = self.rx_result
            elif name == "HAL_SPI_Transmit_DMA": result = self.dma_start_result
            elif name == "HAL_DMA_Init": result = self.dma_init_result
            elif name == "HAL_SPI_Init": result = self.spi_init_result
            cpu.reg_write(UC_ARM_REG_R0, result)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))

        self.cpu.hook_add(UC_HOOK_CODE, intercept)

    def count(self, name):
        return sum(call[0] == name for call in self.io_calls)

    def counter(self, name):
        return int.from_bytes(self.cpu.mem_read(harness.symbol(name)[0], 4), "little")

    def fail_lcd(self):
        self.tx_result = 3  # HAL_TIMEOUT
        self.call("LCD_Transmit", 0x20008000, 1)

    def test_stuck_dma_returns_instead_of_blocking_ui_forever(self):
        self.dma_state = 2
        self.assertEqual(self.call("LCD_WaitDMA"), 0)
        self.assertLessEqual(self.tick, 102)
        self.assertEqual(self.call("LCD_HasFault"), 1)
        self.assertEqual(self.counter("lcdTransferErrors"), 1)

    def test_dma_ready_but_spi_busy_is_not_treated_as_complete(self):
        self.spi_state = 3
        self.assertEqual(self.call("LCD_WaitDMA"), 0)
        self.assertGreaterEqual(self.count("HAL_SPI_GetState"), 1)

    def test_successful_dma_finishes_without_false_fault(self):
        self.assertEqual(self.call("LCD_WaitDMA"), 1)
        self.assertEqual(self.counter("lcdTransferErrors"), 0)

    def test_dma_error_stops_the_current_frame(self):
        self.dma_error = 1
        self.assertEqual(self.call("LCD_WaitDMA"), 0)
        self.assertEqual(self.call("LCD_HasFault"), 1)

    def test_dma_start_failure_does_not_wait_for_a_nonexistent_transfer(self):
        self.dma_start_result = 2
        self.call("LCD_WriteData", 0x20008000, 64)
        self.assertEqual(self.call("LCD_HasFault"), 1)
        self.assertEqual(self.count("HAL_DMA_GetState"), 0)

    def test_transmit_failure_is_latched_and_remaining_pixels_are_skipped(self):
        self.fail_lcd()
        self.call("LCD_Transmit", 0x20008000, 1)
        self.call("LCD_WriteData", 0x20008000, 64)
        self.assertEqual(self.count("HAL_SPI_Transmit"), 1)
        self.assertEqual(self.counter("lcdTransferErrors"), 1)
        self.assertEqual(self.count("HAL_SPI_Transmit_DMA"), 0)

    def test_recovery_is_rate_limited_and_allows_drawing_again(self):
        self.fail_lcd()
        self.tick = 999
        self.assertEqual(self.call("LCD_Recover"), 0)
        self.assertEqual(self.count("HAL_SPI_Abort"), 0)
        self.tick = 1001
        self.assertEqual(self.call("LCD_Recover"), 1)
        self.assertEqual(self.call("LCD_HasFault"), 0)
        self.assertEqual(self.counter("lcdRecoveries"), 1)
        self.tx_result = 0
        self.assertEqual(self.call("LCD_Transmit", 0x20008000, 1), 1)

    def test_failed_reinitialization_keeps_io_fault_latched(self):
        self.fail_lcd()
        self.spi_init_result = 1
        self.tick = 2000
        self.assertEqual(self.call("LCD_Recover"), 0)
        self.assertEqual(self.call("LCD_HasFault"), 1)
        self.assertEqual(self.counter("lcdRecoveries"), 0)

    def test_dma_timeout_handles_millisecond_rollover(self):
        self.tick = 0xFFFFFFF0
        self.dma_state = 2
        self.assertEqual(self.call("LCD_WaitDMA"), 0)
        self.assertLess(self.tick, 200)

    def test_timebase_can_preempt_dma_completion_timeout(self):
        conf = (harness.ROOT / "Application/Core/Inc/stm32f1xx_hal_conf.h").read_text()
        priority = int(re.search(r"#define\s+TICK_INT_PRIORITY\s+(\d+)", conf)[1])
        self.assertLess(priority, 5)
        ioc = (harness.ROOT / "Application/LoRaBasedWPC.ioc").read_text()
        self.assertIn(f"NVIC.TIM1_UP_IRQn=true\\:{priority}\\:", ioc)

    def test_failed_dma_reinitialization_is_not_reported_as_recovery(self):
        self.fail_lcd()
        self.dma_init_result = 1
        self.tick = 2000
        self.assertEqual(self.call("LCD_Recover"), 0)
        self.assertEqual(self.call("LCD_HasFault"), 1)
        self.assertEqual(self.counter("lcdRecoveries"), 0)

    def radio_args(self, length=4):
        self.cpu.mem_write(0x2000F000, struct.pack("<I", length))
        self.cpu.mem_write(0x20009100, b"\xA5" * length)
        self.cpu.reg_write(UC_ARM_REG_R3, 0x20009100)

    def test_radio_failed_address_transfer_returns_zeroed_output_and_releases_cs(self):
        self.tx_result = 2
        self.radio_args()
        self.call("LoRa_readReg", 0x20008000, 0x20009000, 1)
        self.assertEqual(bytes(self.cpu.mem_read(0x20009100, 4)), bytes(4))
        self.assertEqual(self.counter("loraSpiErrors"), 1)
        self.assertEqual(self.count("HAL_SPI_Receive"), 0)
        self.assertEqual(self.count("HAL_SPI_GetState"), 0)
        self.assertEqual(self.io_calls[-1][0], "HAL_GPIO_WritePin")
        self.assertEqual(self.io_calls[-1][1][2], 1)

    def test_radio_receive_error_also_returns_without_state_spin(self):
        self.rx_result = 3
        self.radio_args()
        self.call("LoRa_readReg", 0x20008000, 0x20009000, 1)
        self.assertEqual(self.counter("loraSpiErrors"), 1)
        self.assertEqual(self.count("HAL_SPI_GetState"), 0)

    def test_radio_failed_write_does_not_send_the_data_phase(self):
        self.tx_result = 1
        self.radio_args()
        self.call("LoRa_writeReg", 0x20008000, 0x20009000, 1)
        self.assertEqual(self.count("HAL_SPI_Transmit"), 1)
        self.assertEqual(self.counter("loraSpiErrors"), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
