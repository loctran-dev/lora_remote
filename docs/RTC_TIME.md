# Remote RTC clock

## USB commands

- `TIME\n`: read RTC, without setting it.
- `TIME <decimal timestamp>\n`: set the complete calendar (2000 through 2099).
- Reply: `TIME TS=... DATE=YYYY-MM-DD CLOCK=HH:MM:SS`.
- Setting is refused with `ERR TIME ACTIVE_TIMER` while a timer request is pending
  or any known nondeleted device/module has an active ON timer.
- Other errors: `ERR TIME SYNTAX_OR_RANGE`, `ERR TIME RTC_WRITE`.
- `TIMER` remains a separate command, not a prefix match for `TIME`.

The existing protocol encodes the board's civil calendar as a Unix-style number
without timezone conversion on the MCU. For Vietnam/Thailand civil time, the PC
therefore sends `int(time.time() + 7*3600)`, not plain UTC epoch seconds.

Read only:

```powershell
python tools/set_remote_time.py --port COM6
```

Explicitly set from the PC (UTC+7 by default) and verify within two seconds:

```powershell
python tools/set_remote_time.py --port COM6 --set
```

The PC clock must itself be correct. This is a manual USB clock setter, not an
automatic PC time service. A successful set now schedules unicast GET_STATUS
requests carrying the new time to the paired slaves, in system-task context.
It sends no motor power command and does not rewrite existing history timestamps.

## General setup: Date & time (added 2026-11-19)

Open Menu > General setup > page 3/3 > Date & time. The date/time value is
DD/MM/YYYY HH:MM. The editor has six separately colored textboxes in order:
day, month, year, hour, minute, second.

- LEFT click: increase selected field; RIGHT click: decrease it.
- CENTER click: next field; after seconds, wrap to day.
- Hold CENTER: save to RTC and return to the same General setup page.
- Hold RIGHT: cancel without touching RTC or transmitting a time change.
- ON/OFF buttons do nothing in this editor. Sleep and connection warnings do
  not discard the in-progress edit.
- Operational years: 2024..2099. Day limits follow the selected month/leap year;
  changing month/year clamps an invalid day (e.g. Jan 31 -> Feb 28).
- Values are a frozen editing snapshot; the header clock still runs normally.
- RTC setter rejects active/pending timers rather than changing remote wall time
  during a known countdown. An error is shown without closing the editor.

## Startup status/time synchronization

After loading paired devices and RTC, remote requests one unicast GET_STATUS for
each configured, nondeleted device (hidden and locked devices included). It is
not a broadcast and does not wait for the normal link-silence threshold.
The existing payload carries module=0 and remote civil timestamp; an unset RTC
before 2024 sends zero so it cannot reset a valid slave clock.

The existing supervisor serializes probes, with at most three attempts separated
by ten-second response windows. A matching transaction status reply finishes the
startup probe; an unsolicited status (transaction zero) does not cancel it. Zero
is now reserved from generated remote transaction IDs, including at rollover.
An unavailable slave enters the normal connection-error/cooldown path. Pairing
defers the startup round. Afterwards healthy status traffic does not trigger
periodic polling. Saving a new clock through UI or USB requests a new round.

Slave updates a clock differing by more than the existing 30-second threshold,
whether the motor is OFF or ON. If ON with a deadline, it rebases the absolute
deadline to retain the remaining seconds; an expired deadline stays due. Fresh
RTC read, clock write, deadline rebase and status cache update are protected from
system-task preemption. No relay/power command is issued. Incoming SET_SCHEDULE
continues using its original absolute-deadline logic, not this rebase helper.

Slave USB `TIME` is a read-only diagnostic including timestamp, date/time,
deadline and power. No slave USB setter was added.

## Reset retention

STM32F1 HAL initializes its RAM calendar to January 2000 at each startup even
when the RTC hardware counter survives. `rtc_setting.c` preserves the calendar in
backup registers DR2 (packed date), DR3 (complement), DR4 (commit marker). DR1
retains the existing initialization marker; bootloader DR10 is not touched.
The calendar is restored after RTC initialization, including elapsed whole days
in the hardware counter, and saved when the date changes. No Flash erase/write
is needed. Retention across loss of main power requires the backup domain to
remain powered; a backup-domain reset loses the clock. This is not an atomic
power-loss journal across the HAL midnight-normalization operation.

Clock changes rebase the link monitor's RTC reference without advancing its
uptime, preventing a manual time change from creating a false connection timeout.

## Bench verification, 2026-11-18 (UTC+7)

- Remote-only app built with 0 errors and 0 warnings, 129100 bytes,
  CRC32 `1FC8B698`; flashed over COM6 with CRC verification.
- Set from `2000-01-01 00:37:16` to `2026-11-18 23:58:56`; readback matched PC
  within zero seconds at that sample.
- Restarted through the existing bootloader without erasing/reflashing:
  `2026-11-18 23:59:09`, within one second of the PC.
- CONFIG unchanged; all 14 history entries retained. Slave remained ON without
  an active timer; no slave flash, power or clock command was issued.
- 147 discovered firmware tests passed (7 role-specific skips), including 12
  RTC setter/calendar tests. The 17 schedule UI checks also passed separately.

## Bench verification, 2026-11-19 (UTC+7)

- Remote app: 131068 bytes, CRC32 `D116B6FF`, USB flash CRC verified. Slave app:
  44240 bytes, CRC32 `B6AFF1B5`, standalone ST-Link flash verified; only app
  sectors 0..43 erased. No bootloader or data partition erase.
- Slave config before/after flash SHA256 matches:
  `E01FC106C6E80F6232426352D68691A6C3D44A606692B60DA6931565122FBF1D`.
- Slave initially read `2000-01-01 00:50:52`. Remote startup sent exactly one
  status request (`REQUESTS=1 OK=1 FAIL=0`); both read `2026-11-19 00:12:49`.
- Native UI day change then Cancel retained the live date. Hour +1 then Save
  changed remote and slave to 01:13:48/01:13:50. USB PC-time setter restored
  UTC+7 on remote and automatically on slave, exercising forward/backward sync.
- Restart through bootloader retained remote date/time, matched PC within 1 s,
  and again sent exactly one successful startup request. Slave stayed OFF after
  the explicit OFF-before-flash test action. 15 histories retained on each side.
- 165 remote discovered tests passed (7 role skips); 70 slave tests passed
  (6 role skips), plus ten explicit scheduler checks. Timer-preservation cases
  while ON, leap dates, invalid input and timeout failures were ARM-emulated;
  the physical clock/UI tests above were performed with the motor OFF.
- LCD/RF SPI diagnostics zero errors. Snapshots: dist/date-time-general.bmp and
  dist/date-time-editor.bmp. The separate intermittent USB hang is not claimed
  solved by this feature.

