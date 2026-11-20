# Remote hang hardening - 2026-11-09

Installed remote app: 112324 bytes, CRC32 54099494, verified over COM6.
Bootloader and slave firmware unchanged. Current configuration preserved:
SLEEP=1, UPDATE=5, SLEEP_UPDATE=5, IDLE=45, LOST=180, AUTO(device 1)=2,
NET=1, FREQ=433, SF=7. Motor stayed OFF throughout testing.

## Evidence and limits

User reported intermittent hangs, probably when switching screens. The remote
was responsive when inspected. No fault-register dump of the original physical
hang was available; these changes fix concrete lockup paths found in the code,
not a claim that every possible spontaneous fault has been reproduced.

- LCD waited forever for DMA READY, ignored DMA start errors and used infinite
  blocking SPI timeouts. A failed transfer while holding UI ownership blocks
  screen changes and USB commands that run in systemTask behind the same mutex.
- The STM32 HAL SPI DMA-complete handler uses HAL tick timeouts to wait for
  TXE/BSY. DMA IRQ priority 5 could previously prevent the priority-15 TIM1
  timebase from running, so those timeouts could never expire on stuck flags.
- LoRa did unbounded READY spins after already-blocking HAL SPI calls. An ERROR
  state could spin forever, starving lcdTask. TX, RX and pairing setup also
  accessed the same radio without mutual exclusion.

## Changes

- LCD blocking TX and DMA completion now have 100-ms bounds, check return/error
  states, and wait for both DMA and SPI completion. DMA disable polling is bounded.
- First LCD fault skips the remainder of that frame, avoiding one timeout per
  pixel. lcdTask attempts bus recovery at most once per second: abort DMA/SPI,
  reinitialize DMA, reset SPI2 and restore its configuration. Successful recovery
  redraws existing widgets without reinitializing the editor or losing its text.
- Corrected 8/16-bit buffer advancement in multi-chunk transfers.
- HAL TIM1 priority is 4, above DMA priority 5. Its callback only increments
  uwTick and never calls FreeRTOS. CubeMX .ioc mirrors this priority; SysTick and
  RTOS-using peripheral IRQ priorities were not raised.
- Removed LoRa READY spins; failed reads return zeroed data, CS is released,
  and a SPI error counter increments. Register-transfer timeouts are 100 ms.
- A priority-inheriting radio mutex serializes complete TX/RX/pairing operations.
  Radio code never acquires the UI mutex, avoiding reverse lock ordering.
- Added read-only USB DIAG: FreeRTOS current/minimum free heap, remaining stack
  high-water marks in BYTES for each task, LCD error/recovery/fault state and
  LoRa SPI error count. Statistics reset on reboot; no automatic board reset
  was added to conceal failures.

## Verification

- 79 remote host/ARM-code tests pass, including 14 new I/O tests: permanent DMA
  BUSY, SPI still BUSY, DMA/SPI errors, failed DMA start, fail-frame latching,
  recovery/reinitialization failure, recovery cooldown, tick rollover and HAL
  tick/DMA priority consistency. These execute built ARM code with mocked I/O.
- Before the patch, a short baseline stress test also passed: the original
  spontaneous hang was not reproduced by that short run.
- New firmware: tools/check_remote_ui.py --run --rounds 100 completed 400 screen
  transitions plus page navigation in 128.8 seconds, without a hang or config
  change. Automatic STATUS reception continued (STATUS=46, RX_AGE=2 at uptime
  138 s). LCD_ERR=LCD_REC=LCD_FAULT=RF_SPI_ERR=0 throughout normal operation.
- Opened/canceled all eight General editors and all seven Device editors.
  Saved Auto status=2 unchanged to exercise remote TX plus slave ACK/status RX.
  Slave still reported MODE=2, NET=1, NODE=1, I=0, D=0, A=2, PWR=0 afterward.
- LCD frame capture succeeded: dist/hangfix-ui.bmp; USB remained responsive.
- Heap stayed at 1656 bytes free/minimum. Lowest observed remaining task-stack
  margins: SYS=440, LCD=616, TX=448, RX=656, KEY=420, BEEP=352 bytes.

Physical DMA/SPI faults were NOT injected on the board, so real bus-recovery
behavior under electrical failure remains unverified. Sleep/RTC wake was not
retested in this patch. If the original hang recurs, preserve the state if
possible and collect DIAG, LINK 1, current screen and triggering button sequence
before resetting; a still-unresponsive USB port will require debugger capture.
