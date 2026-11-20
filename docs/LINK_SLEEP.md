# Autonomous status, link supervision and sleep

Implemented 2026-11-09. This supersedes the old disabled-sleep/polling notes.
Application-only changes; both USB bootloaders and Flash partitions are unchanged.

## Settings

| UI field | Location | Range (seconds) | Default |
| --- | --- | --- | --- |
| Auto status interval | Device settings, item 7 | 0..9999; 0 disables | 60 |
| Sleep after idle | General settings, item 7 | 1..9999 | 60 |
| Link timeout | General settings, item 8 | 1..9999 | 180 |
| Sleep check interval | General settings, item 3 | 1..9999 | 300 |
| Link scan interval | General settings, item 2 | 1..9999 | 5 |

The last field controls LOCAL awake scans, not periodic radio requests. Radio
reception also requests a scan. Choose Auto status interval below Link timeout
with margin for packet loss; otherwise confirmations are expected even on a
healthy radio. Sleep enable remains a separate setting, default disabled.
Both General and Device settings use four two-line rows per page; see
GENERAL_UI.md and DEVICE_UI.md. Numeric editors retain the character highlight.

## Slave

Every successful STATUS transmission, including a response to a remote request,
updates lastStatusTxTick. The 1-second timer queues an unsolicited STATUS when
the configured interval has elapsed since that transmission. Other traffic does
not restart this interval. TX failure does not advance it. A pending report is
not duplicated by the timer. Boot schedules the first report promptly; network
configuration mode and unconfigured addresses suppress autonomous reports.
The report uses CMD_GET_STATUS / RESPONSE, the configured network/node and
transaction 0. Existing solicited replies retain the request transaction.

SET_CONFIG now sends a packed 7-byte payload: module:u8, threshold:u16,
delay:u16, autoStatusSeconds:u16, little-endian. Slave also accepts legacy
5-byte requests, preserving its current auto period. Success ACK follows Flash
commit. USB MODE adds A=seconds. Remote local edits are intent, not proof of ACK;
there is still no automatic configuration resynchronization after a lost ACK.

## Remote connection supervision

Maintain per-node last valid RX and last STATUS times. Deleted/unconfigured
entries are excluded; Hidden and Locked devices remain supervised.

1. Awake local scan, RTC wake or received radio packet requests inspection.
2. RX age <= Link timeout: no health-check packet is sent.
3. RX age > Link timeout: queue STATUS request 1.
4. Wait 10 seconds AFTER the sender finishes TX; repeat for attempts 2 and 3.
5. Only after the third reply window expires, mark connection error, wake the
   display and beep. ACKs for other commands do not confirm STATUS availability.
6. A valid fresh STATUS cancels an active confirmation and clears its error.

One node is confirmed at a time, with round-robin fairness across offline nodes.
Queue backpressure does not consume an attempt. A failed radio TX consumes an
attempt but still gets a 10-second window; queued-but-not-dispatched attempts
have a 10-second fallback deadline. A confirmed failed node is retried after a
Link-timeout cooldown. Other offline nodes can extend the wait before a given
node's first probe. No timeout path sends motor ON/OFF or changes motor outputs.
Visible failures select their device unless an editor is open. Hidden failures
still beep/wake but do not make that device appear on the control screen.

## Low power

After user inactivity, the remote enters STM32 STOP with LoRa powered in
continuous RX. A slave packet can therefore wake it via DIO0. RTC periodically
wakes it to inspect ages; neither RTC nor RF resets the user-idle timer or
turns the LCD on just to receive a healthy report. Confirmations keep the CPU
awake for all reply windows; a confirmed fault keeps the display awake until
user interaction. A valid slave alarm also wakes the display.

STOP is inhibited while USB SOF traffic is active, editing/pairing, saving Flash,
holding a button, or processing queued radio/buzzer work. ADC/TIM8 are stopped
and restarted. The PLL is restored before interrupts resume. RTOS time pauses
in STOP; link age uses monotonic RTC-derived seconds instead of RTOS ticks.
Reply deadlines use millisecond HAL ticks while STOP is inhibited.

Hardware limitation: LEFT PA9, RIGHT PC8 and OFF PC7 can wake STOP. CENTER PC9
shares EXTI9 with LEFT; ON PA8 shares EXTI8 with RIGHT. Those two cannot directly
wake STOP with this wiring. Use a supported wake key first. LoRa RX consumption
remains present; this is not the minimum possible radio power configuration.

UI render, window changes and LCD readback share a priority-inheriting mutex.
Do not bypass it from a new task or ISR.

## Persistence and diagnostics

Remote appends a validated LNK1 extension to the existing 836-byte storage
prefix (885 bytes total); old field offsets and all pairings remain unchanged.
Legacy/invalid extensions receive the defaults above. Slave keeps its 20-byte
CFG1 journal, packing the period and A5 tag into the old checksum word; legacy
records default to 60 seconds. Checksums are the project's byte-sum convention,
not a cryptographic checksum. Old slave firmware is not downgrade-compatible
with extended records. Do not mass-erase settings or flash factory.hex to update.

USB remote LINK or LINK 1..20 reports per-device RX/STATUS ages, ERROR, active
probe/TRY/STATE, millisecond DEADLINE, global request/confirmation/failure/status
counters, logical MODE and STOP/RTC/RF/KEY wake counters. CONFIG adds IDLE, LOST
and selected-device AUTO. Counters/ages reset at reboot; the settings persist.

## Verification status

Built ARM-code emulation covers healthy silence, stale-only requests, 3 x 10 s,
ACK versus STATUS, cancellation, recovery, hidden/deleted nodes, backpressure,
round-robin fairness, RTC midnight, settings compatibility, logical sleep policy,
slave queue-preemption race, failed TX and UTC calendar conversions. Run:

    python -m unittest discover -s tools -p 'test_*.py'

USB hardware checks confirmed the extended SET_CONFIG reaches slave (A=2),
STATUS repeats with motor OFF, and settings editors save IDLE/LOST/AUTO. During
earlier LCD readback remote stopped responding; a shared UI mutex was added.
After power cycle and app reflash, readback succeeded and USB remained responsive.
Image: dist/general-link-fixed.bmp.

The hardware loss/recovery test passed with LOST=5 and A=2: first request at
uptime 10 s (local scans every 5 s), second around 20 s, third around 30 s;
ERROR stayed 0 throughout all three windows and became 1 around 40 s. After
returning slave to RUNNING, fresh STATUS cleared ERROR by uptime 42 s.
Motor PWR=0 throughout. Script: tools/check_link_loss.py --run (requires those
explicit bench settings; restores slave RUNNING in finally). Other paired
devices are absent, so REQUESTS can increase as the next node's test starts.
After restart with LOST=180 and slave A=2, STATUS=32 and REQUESTS=0 were observed
by uptime 99 s. With USB connected, MODE=2 and STOP=0 despite IDLE=5/SLEEP=1,
confirming the USB sleep guard. Physical STOP/RTC/RF wakeup is still awaiting
the USB-detach test; emulated sleep alone is not proof of physical wakeup.

Installed: remote 110628 bytes / CRC32 B47BECA4 (UI mutex + fair scans),
slave 39796 bytes / F8A7A02A. Both CRCs were verified over USB.
The subsequent General UI update is 111020 bytes / 2114A083; see GENERAL_UI.md
for its deployment checks and current preserved configuration.
The following Device UI update is 111028 bytes / 9E48655C (DEVICE_UI.md).
Latest hang-hardening app: 112324 bytes / 54099494; see HANG_FIX.md for
finite I/O waits, radio serialization, diagnostics, validation and limits.
50 remote + 36 slave tests pass. Bootloaders were not programmed.

Bench state awaiting USB-detach sleep test: remote SLEEP=1, UPDATE=5,
SLEEP_UPDATE=5, IDLE=5, LOST=180, AUTO(device 1)=2. Slave RUNNING NET=1 NODE=1,
I=0 D=0 A=2, PWR=0. Original defaults to restore: SLEEP=0, SLEEP_UPDATE=300,
IDLE=60, LOST=180, AUTO=60 on both boards. All eight pairings/flags are preserved.
Do not accidentally restore CONFIG mode on the slave: it has already been
returned to RUNNING. Restore the bench settings after the physical sleep test.
