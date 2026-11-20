# Confirmed timer commands - 2026-11-17

## Problem and scope

The previous physical one-minute test failed: ON worked, but the display stayed
Not set and no automatic OFF occurred by 65.2 seconds. OFF cleanup was confirmed.
The exact cause of that failed RF exchange is not established yet. Source review
confirmed a robustness defect: the timer request was sent only once and its ACK
result was discarded. A successful USB BTN reply only meant the UI callback ran.

This change fixes that silent/unconfirmed submission path in the remote only.
Slave source, bootloaders, pairings and saved settings are unchanged.

## Behavior

- Track destination, module, transaction ID, sync time and absolute stop deadline.
- Retry up to three total transmissions, spaced three seconds after transmission.
  All attempts use the same transaction, payload and deadline; retries cannot
  extend the timer. Do not retransmit an already-expired deadline.
- Accept ACK only for the matching network, node, module and transaction.
  Result 1 confirms the deadline; result 0 reports Rejected and stops retrying.
  A matching STATUS also confirms the schedule. A late matching reply/STATUS can
  recover a timeout, but cannot revive a canceled/replaced command.
- Keep the known timer data unchanged while waiting or on failure. After a
  successful matching ACK, cache the confirmed deadline for immediate display;
  subsequent STATUS remains authoritative about motor power and deadline.
- Show Sending, Rejected, No reply or Queue err instead of silently showing
  Not set. Hold the remote awake while a timer command is outstanding.
- Explicit ON/OFF for that node invalidates pending timer retries, and queued
  obsolete timer packets are filtered before transmission.

Read-only USB command `TIMER` returns NOW, cached PWR/DEADLINE, request STATE,
NODE, transaction ID, TRY count, TX_OK count, SYNC and TARGET. States are
0 idle, 1 waiting, 2 confirmed, 3 rejected, 4 timeout, 5 queue error.

## Verification and deployment status

Keil app build: 0 errors / 0 warnings. dist/application.bin is 120972 bytes,
CRC32 5F20D2C3. Regression suite: 109 tests, plus six explicit schedule-wire checks.
New tests exercise exact retry payloads, no deadline extension, bounded attempts,
matching/rejected/stale/late replies, full queue, OFF cancellation, expired
deadlines, UI feedback and the sleep guard.

FLASHED AND RETESTED: after the user's physical remote reset, flashed REMOTE APP
ONLY over COM6 and verified CRC32 5F20D2C3. Neither bootloader nor slave was changed.
Two live no-load 00:01 schedules were accepted and automatically stopped the slave.

First run: transaction 2, SYNC=946713591, TARGET=946713651, STATE=2, TRY=1,
TX_OK=1. Captures show 00:00:59 then 00:00:37. An extra independent screenshot
process briefly occupied COM6 and interrupted the test script, including its
OFF cleanup attempt. This was a PC port-ownership mistake, not a firmware failure.
A read-only observer resumed tracking the SAME schedule without another ON or
timer command. It observed slave OFF at remote time 946713653 (62 s after SYNC),
then explicitly sent OFF and verified PWR=2. No schedule was extended/restarted.

Second run: single-process automated script with --capture-prefix
dist/timer-confirmed-clean completed PASS (exit 0). Transaction 5,
SYNC=946713692, TARGET=946713752, STATE=2, TRY=1, TX_OK=1. Screenshots show
00:00:59 and 00:00:32; the slave became OFF at elapsed 60.2 s. Final cleanup
confirmed STATUS PWR=2 and unchanged CONFIG. Captures are
dist/timer-confirmed-clean-early.bmp, -middle.bmp and -off.bmp.

Both native exchanges succeeded on their first transmission. Lost-packet retry,
rejection and timeout branches were checked by ARM regression tests, not induced
as RF faults on the bench. The exact reason for the earlier single failed
exchange remains unproven; the new confirmed-command path and countdown/auto-OFF
have now passed physically. This does not establish a fix for the separate
intermittent remote USB hang.

The user explicitly says the bench is by default NOT connected to a load and
authorizes further test runs without asking again each time. Recheck that
assumption if the user changes the hardware/load setup. Keep tests bounded,
verify initial OFF, and always verify final OFF.

Reproduction (one process must own COM6; do not run extra capture/console commands
against it while the bench script is running):

```powershell
python tools/usb_update.py dist/application.bin --port COM6 --request
python tools/check_schedule_hardware.py --allow-on --capture-prefix dist/timer-confirmed
```

The updated bench script checks exact 60-second payload, confirmation/deadline,
automatic OFF, unchanged CONFIG and final OFF. It optionally captures early,
middle and final LCD frames. A rejected/timed-out request ends the test early
with OFF cleanup instead of waiting the full minute.
