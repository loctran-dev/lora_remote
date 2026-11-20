# Hardware verification — history / ST-Link migration — 2026-11-18

Final installed applications:
- Remote: 128024 bytes, CRC32 D6D57EAB, USB update verified, existing bootloader retained.
- Slave: 43516 bytes, CRC32 B29FF455, ST-Link HEX at 08000000, sector erase 0..42,
  download verified. Custom slave USB bootloader removed from the execution path.
  Original full 64 KiB Flash backup is retained (see STLINK_STANDALONE.md in slave).
  Current factory.hex is standalone; original USB factory image archived separately.

Configuration remained unchanged:
remote NET=1 FREQ=433 SF=7 SLEEP=1 UPDATE=5 SLEEP_UPDATE=60 IDLE=45 LOST=180 AUTO=2;
slave MODE=2 NET=1 NODE=1 I=0 D=0 F=433 SF=7 A=2.
The final slave FC00..FFFF was uploaded again to config-final.bin and compared
byte-for-byte with the pre-migration backup: identical.

## End-to-end push and ACK

First safe test: remote sent OFF while already OFF. Slave appended event ID 1,
remote durably stored exactly one event and ACKed; slave PENDING became 0.
Decoded event: ACTION=OFF SOURCE=Remote BEFORE=OFF AFTER=OFF RESULT=No change.

An initial test generator sent three RF control commands too quickly; only two
new OFF commands reached the slave. One history packet also needed a resend.
The two events that actually existed were eventually stored/ACKed, with no
duplicate records. This is not evidence of a third history event being lost:
a USB BTN success only confirms callback dispatch, not delivery of the control
command. The test generator was corrected to wait for the slave's LAST event
ID after each OFF before generating the next action.

Corrected OFF-only burst test passed:
- before: three retained events, no pending outbox.
- after: six events at both sides, slave PENDING=0, three additional ACKs.
- observed send-counter changes at 0.34, 9.95 and 20.75 seconds; polling is coarse
  and the first send preceded the first observation. Exact >=10000 ms gating
  is verified separately by ARM tests at the 9999/10000 boundary.
- no DROP/Flash errors, no configuration change, motor remained OFF.

## Countdown / real before-after states

Initial history firmware passed the one-minute timer test, with final OFF
observed at elapsed 64.5 s (5 s poll spacing). Recorded ON from Remote and OFF
from Timer were confirmed from saved EVENT records.

The FINAL installed B29FF455 slave image passed another one-minute timer test:
SYS Sync=946771166, deadline=946771226, remote ACK state confirmed, one transmit.
Slave was ON at the 58.9 s sample and OFF at the 64.1 s sample (5 s polling).
Explicit OFF cleanup then verified PWR=2.

Decoded FINAL records on the remote:
- ID 11, timestamp 946771161: ON / Remote / OFF -> ON / Applied.
- ID 12, timestamp 946771226: OFF / Timer / ON -> OFF / Applied.
- ID 13, timestamp 946771228: OFF / Remote / OFF -> OFF / No change.
All flags=TIME_UNKNOWN because the RTC is still in year 2000. No PC clock was
written, and UI explicitly shows Time not synced rather than a fabricated date.

Final native state: remote COUNT=13, no Flash errors; slave COUNT=13, LAST=13,
PENDING=0, DROP=0, ERR=0, FULL=0, PWR=2.

## Persistence

Reset both boards after nine records were synced. Remote retained COUNT=9 but
runtime STORED/ACK counters reset to 0. Slave retained COUNT=9/LAST=9/PENDING=0,
with SENT=0 even after the startup quiet period. No synced event was replayed.
A new OFF produced ID 10, not reused ID 1. Subsequent sector-only slave app
updates preserved all ten records and pending/synced markers.

## UI and resource checks

Actual LCD captures in remote dist/:
- history-device-picker.bmp
- history-list-first.bmp / history-detail-first.bmp
- history-list-final.bmp / history-timer-detail.bmp / history-on-detail.bmp
- history-delivered-list.bmp
- history-final-timer-early.bmp / -middle.bmp / -off.bmp

Physical UI confirmed picker -> four-row list -> detail -> back. Timer detail
shows Source: Timer, Before: ON, After: OFF, Result: Applied. ON/OFF long presses
in History did not control the motor or add actions. Twenty further detail/back
cycles completed without changing history, settings, heap or motor state.
Remote was left on the history list for review.

Final diagnostics:
remote HEAP=1664 MIN_HEAP=1664 STACK_SYS=448 STACK_LCD=648 STACK_TX=448 STACK_RX=632;
LCD_ERR=LCD_REC=LCD_FAULT=RF_SPI_ERR=0.
slave HEAP=2160 STACK_SYS=848 STACK_RX=688.

## Automated verification

Remote: 135 discovered tests, OK (7 role-specific skips), plus six schedule-wire
checks. Includes UI navigation, hidden/empty device cases, stable selection,
calendar conversion, full event CRC, durable ACK and retained dedup cursors.
Slave: 59 discovered tests, OK (6 role-specific skips), plus all ten scheduler
checks. Includes persistent ID/synced flags, full-outbox protection, rollover,
FIFO/10-second retries, invalid ACK rejection, late-ACK/queued-retry cancellation,
IO hook capture and active-error snapshots on power actions.

Forced Flash faults, corrupted packets, full journal capacity and IO paths were
tested in ARM emulation; they were not induced physically on this bench.
The user explicitly requested NO active IO input handling yet. Source hooks are
ready, but no IN1/IN2 mapping or additional DO relay output was enabled.

The separate intermittent remote USB hang is not declared solved by these tests.
See HISTORY.md for finite retention, overflow and manual stream-erase limitations.
