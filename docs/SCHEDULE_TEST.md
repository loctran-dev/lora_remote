# Timer verification

For the 2026-11-17 remote UI changes and current build/test/flash status, see
[SCHEDULE_UI.md](SCHEDULE_UI.md).

## Confirmation fix hardware retest - 2026-11-17: PASS

Remote APP ONLY flashed and CRC32 5F20D2C3 verified. Two actual no-load 00:01
schedules produced countdowns and automatic OFF. The clean automated run passed
at elapsed 60.2 s, with frames 00:00:59 and 00:00:32. Matching ACK/state/deadline
were verified; TRY=1 and TX_OK=1. Cleanup confirmed slave PWR=2, CONFIG unchanged.
Neither bootloader nor slave was programmed. See
[SCHEDULE_CONFIRMATION.md](SCHEDULE_CONFIRMATION.md) for exact timestamps,
screenshots and the PC port conflict during the first run (resumed safely).

## New UI hardware countdown test - 2026-11-17: FAIL

This is the historical failure before the confirmation/retry changes. See the
PASS above for the subsequent flashed build and live retest.

User confirmed no connected load and approved ON for a one-minute test. Remote
APP CRC32 BAA4469C; slave metadata read via SWD Hot Plug still identifies APP
39796 bytes / CRC32 03E2532F. No firmware was changed during this test.

Ran `python tools/check_schedule_hardware.py --allow-on`: slave initially PWR=2,
remote ON was received (PWR=1), then custom HH:MM 00:01 was submitted through the
UI. Captures dist/timer-ui-live-countdown-a.bmp and -b.bmp show ON but
Auto stop / Not set, not a countdown. Remote LINK 1 still showed fresh STATUS
(STATUS_AGE=1, RX_AGE=1, ERROR=0); LCD and RF SPI fault counters remained zero.
Slave stayed PWR=1 through elapsed 65.2 seconds: automatic OFF did not occur.

The test's finally block sent remote OFF and confirmed slave STATUS PWR=2.
CONFIG was unchanged. Do not treat the earlier successful 2026-11-15 test or
the ARM-only UI tests as a PASS for this new physical run.

Cause is not yet isolated. The received STATUS did not expose an active timer
to the UI. Source review shows submission returns to Control after queueing the
request; remote handleSetSchedule reads the response but does not use its result
or surface successful/rejected/missing confirmation. A successful USB BTN reply
therefore does not prove that the slave accepted the timer. Do not conclude a
specific RF loss or slave scheduling defect without further evidence. A new
physical ON run requires fresh approval.

## Retest after the user's fix - 2026-11-15

User changed slave commandHandlers.c to compare payloadRequest.scheduleTime == 0
instead of sysInfo.scheduleTime == 0. That edit was preserved as written.
Built and flashed SLAVE APP ONLY: 39796 bytes, CRC32 03E2532F verified over COM7;
0 build errors/warnings. Remote app and both bootloaders were not programmed.

User again confirmed no connected load and approved a one-minute ON test.
Native retest PASS: custom 00h01 was applied, remote countdown was captured at
00h00m53 and later 00h00m21, then slave transitioned OFF between polling samples
at 57.2 and 62.4 seconds. Finally remote OFF was also sent and slave PWR=2 verified.
The saved screenshots are dist/schedule-fixed-countdown-a.bmp and
dist/schedule-fixed-countdown-b.bmp.

No remote UI edit was necessary to make the running countdown appear. The prior
installed slave still had the old bug and reported scheduleTime=0, which the UI
correctly hides. A nonzero deadline must arrive in STATUS before the countdown
can appear. ACK alone does not update it, so appearance can still wait for Auto
status interval; current bench value is 2 s. Setting a timer while motor is OFF
is rejected by the current slave handler and will not start a countdown.

Verification: 36 normal slave tests pass; explicit slave scheduler checks now
pass 9/10 (fresh timer fixed). The remaining failed check is clearing scheduleTime
after automatic stop: the user's edit did not change main.c's expiry cleanup.
79 normal remote tests plus 6 explicit remote schedule checks pass.

Preserved current CONFIG: NET=1, FREQ=433, SF=7, SLEEP=1, UPDATE=5,
SLEEP_UPDATE=60, IDLE=45, LOST=180, AUTO(device 1)=2. Final motor state OFF.
No additional firmware fix was made for the outstanding expired-timer issue.

## Historical test before the fix

Result: FAIL on the current installed firmware. No application or bootloader
was rebuilt, modified or flashed. Only diagnostic scripts/report were added.

## Physical bench test

User confirmed the slave board has no connected load and explicitly authorized
ON for a one-minute timer test. Initial observed slave power was ON; sent remote
OFF first and confirmed slave PWR=2 before starting the controlled test.

1. Sent ON using the remote USB button simulator; slave reported PWR=1.
2. Used Set time -> custom value 00h01 -> hold CENTER to apply.
3. Polled slave STATUS until 65.2 seconds after applying the timer. It stayed
   PWR=1 throughout; no automatic OFF was observed.
4. Finally sent remote OFF and verified slave PWR=2. A subsequent independent
   STATUS read also confirmed PWR=2.

Remote remained responsive and current configuration was unchanged:
NET=1, FREQ=433, SF=7, SLEEP=1, UPDATE=5, SLEEP_UPDATE=60, IDLE=45,
LOST=180, AUTO(device 1)=2. Slave MODE=2, NET=1, NODE=1, I=0, D=0, A=2.
Do not restore SLEEP_UPDATE=5 from older reports; current user value is 60.

Script: tools/check_schedule_hardware.py --allow-on. Explicit no-load approval
is required; it starts on normal M11 Control and sends OFF in finally.
Screenshots: dist/schedule-test-before.bmp, dist/schedule-test-final-off.bmp.

## Isolated tests of the built ARM code

Remote tools/check_schedule.py: 6 checks pass. No physical I/O:

- 00h01 sends SET_SCHEDULE to Net 1 / Node 1 / module 0, deadline now + 60.
- 30-minute preset sends now + 1800.
- Off sends deadline 0.
- Hidden/deleted devices do not send a timer request.
- Holding CENTER on the Off preset also sends cancellation, not merely Back;
  this documents current behavior, not approval of its usability.

Slave ../Lora_device/tools/check_schedule.py: 10 checks, 8 pass and 2 FAIL.
The failing assertions intentionally preserve evidence of uncorrected defects;
these explicit check_ scripts are not part of normal test_ discovery.

## Confirmed defects

1. Slave commandHandlers.c:266 tests the OLD sysInfo.scheduleTime == 0 when
   deciding to clear a schedule. ON always resets that old field to zero.
   Therefore the first future timer after ON is discarded, yet result=1 is
   returned. ARM check: requested deadline 1060, stored deadline 0, ACK result 1.
   This explains the physical test failure; the independent radio packet path
   was not captured over the air during the bench run.
2. Slave main.c:776 auto-OFF logic works when an existing nonzero deadline is
   preseeded in emulation, but does not clear scheduleTime after expiration.
   The expired value remains in STATUS and can leave the remote timer visible.

Replacing a preexisting timer, cancellation, OFF-state rejection, invalid
module/address/length checks, pre-deadline ON and expiry OFF checks pass.
ON states in ARM tests are memory only; they never energize a board.

Suggested next work (not implemented): compare the REQUESTED deadline to zero,
clear the timer after auto-OFF and report fresh STATUS; then rerun both explicit
checks and the no-load one-minute bench test. Timer return/cancel UX should also
be separated from committing a selected preset.
