# Known issues

Open problems found in the code but not yet fixed. Read this before changing
motor/module handling, schedules, config storage or the screens listed below.

Source: code review of commits 467cf2c..b881d5f (2026-11-26, UTC+7), which moved
the remote from one motor per device to a shared motor pool (CFG3) and made the
control screen step through motors. Line numbers refer to b881d5f and may drift.

When an issue is fixed, move it to "Fixed" with the date and commit. Add new
issues under "Open" with the same fields.

## Open

### KI-14: config_store host test has not been run since CFG3

- Where: `tools/config_store_host/test_config.c` (`tools/test_config_store.py`).
- Problem: it needs Linux and gcc; on this Windows bench it is skipped. It was
  edited for the CFG3 layout and the removal of the old formats but never
  compiled or run.
- Effect: the power-loss-at-every-flash-operation coverage is unverified for
  CFG3 (the ARM tests in tools/test_config_load.py cover one interrupted save).
- Fix: run it on a Linux machine or in WSL with a distribution installed.

### KI-15: History can only be viewed per device, not per motor

- Where: `Application/Library/screen_hs.c` (picker, list, ScrnHs_OpenDeviceHistory).
- Problem: the picker lists devices ("M1", "M2") and the list shows every
  event of the chosen device, all motors mixed. Opening history from the
  control screen while M12 is shown also opens the whole device's list.
  Since KI-12 each event records its motor (list prefix "M12 " on devices
  with several motors, detail title "M12 Event"), but nothing filters by it.
- Effect: on a device with several motors the user cannot see the history of
  one motor only.
- Fix (proposed): when opened from the control screen, show only the events
  of the motor on screen plus the device's link events; from the main menu
  keep the per-device view. Alternative: list motors ("M11", "M12", ...) in
  the picker instead of devices.

Found 2026-11-27 reviewing multi-motor handling at f70ff46; line numbers refer
to that commit.

### KI-16: Timers on two motors in a row lose the first request

- Where: `Application/Library/schedule.c` (single `scheduleRequest`,
  Schedule_Track, Schedule_ShouldTransmit:82, Schedule_HandleResponse).
- Problem: only one schedule request is tracked for the whole remote.
  Schedule_Track() replaces it, and Schedule_ShouldTransmit() drops every
  queued CMD_SET_SCHEDULE that is not the tracked one.
- Effect: set a timer on M11, step to M12 and set one there. If M11's request
  is still queued it is never sent; if it was sent, its reply is rejected, so
  there is no beep and no 3 s resend, and M11 only shows the timer after its
  next status. Rare with one motor per device, easy now on one device.
- Fix (proposed): track one request per motor (small array or queue), or
  refuse a new timer while Schedule_IsWaiting().

### KI-17: Alarms are easy to miss and show no details (needs an alarm screen)

- Where: `Application/Library/command_handlers.c:395-404` (handleAlarmMessage),
  `Application/Library/power_sleep.c:86` (Power_WakeForError).
- Problem: the alarm only turns the backlight on; the control screen keeps
  the selected motor and shows no notice. A lost link has
  ScrnDcs_ShowConnectionError(), an alarm has nothing similar.
  `alarmReported` is set TRUE and never cleared.
- Effect: while M11 is shown, an alarm on M12 lights the display with nothing
  to say why. Once a motor reported an alarm it shows the alarm until reboot.
  The control screen only shows "ALARM" and a triangle: `alarmCode` and
  `alarmDevTime` are stored (command_handlers.c:403-404) but never displayed,
  and there is no way to acknowledge an alarm.
- Fix (decided 2026-11-27): an alarm screen.
  - When a motor reports an alarm, open an alarm screen over the current one
    with the details: the motor (`D1 M2. Motor nuoc`, see KI-22), the alarm
    (lost input power / lost motor power / phase currents differ, from
    `alarmCode`) and when it happened (`alarmDevTime`, device clock).
  - The buzzer sounds continuously (`eBuzzer_Mode_AlarmLong`) while the
    screen is open.
  - CENTER acknowledges: buzzer off, the alarm screen closes.
  - If the remote was on device control, it comes back showing the alarmed
    motor (curDevice/curModule set to it); otherwise it returns to the
    screen it was on.
  - Several alarms: queue them and show them one by one; each CENTER
    acknowledges the one on screen and opens the next.
  - While an editor or pairing is open the remote does not switch screens:
    the status bar shows "ALARM" and the buzzer sounds; the queued alarm
    screen opens by itself once the user leaves the editor/pairing.
  - Acknowledging only silences and closes the screen. `alarmReported` (the
    "ALARM" status on the control screen) stays until the device reports the
    alarm cleared.
  - A hidden motor raises nothing: no screen, no buzzer, no status-bar text.
  - Clearing `alarmReported` (decided 2026-11-27, option A+C):
    - C (main): the slave sends CMD_ALARM_MESSAGE with the new code
      ALARM_CLEARED (0x80) for that module when the alarm clears; the remote
      clears `alarmReported` on it. Code 0 (ALARM_NONE) is never treated as
      a clear: today's slave sends 0 for real alarms (see KI-23), so a
      remote that read 0 as "cleared" would never show their alarms.
    - A (fallback): the slave repeats an active alarm every 15 s. If no
      CMD_ALARM_MESSAGE for that motor arrives for 60..90 s while the link is
      up, the remote clears `alarmReported` itself. Never while `linkLost`.
      This timeout depends on the slave's 15 s period; keep it well above it
      and note the coupling in both repos.
    - An alarm that comes back soon after A cleared it (weak signal, three
      repeats lost) only sets "ALARM" again: no alarm screen, no buzzer.
    - Log on the USB console whether C or A cleared an alarm.
    - Compatibility: new remote + old slave still shows alarms (code 0,
      type unknown) and A clears them; old remote + new slave shows
      ALARM_CLEARED as an alarm, so update the slave first (as KI-11).
    - Open: an alarm that clears before the user acknowledged it: keep the
      alarm screen until CENTER (marked "cleared"), or close it by itself.
  - Slave changes needed: KI-23.

### KI-18: Motor slots are not checked against the factory module count on load

- Where: `Application/Library/config_store.c:370-389` (sanitizeMotors),
  `Config_ModuleCount()`.
- Problem: sanitizeMotors frees only slots with `module >= MAXIMUM_MODULE`,
  not `>= factoryInfo.moduleCount`, and recreates only module 0 of a device.
  A device reporting more than 6 modules is silently limited to 6.
- Effect: a corrupted or old slot can show a motor that does not exist (e.g.
  M14 on a 2-motor device); a lost slot of module 1..n-1 removes that motor
  without notice; pairing shows "Modules 08" but only 6 can be used.
- Fix (proposed): free slots with `module >= Config_ModuleCount(moduleCount)`
  and recreate the missing ones in sanitizeMotors; tell the user at pairing
  when a device has more motors than MAXIMUM_MODULE.

### KI-19: Device manager Hide/Show overwrites the per-motor Hide setting

- Where: `Application/Library/screen_dvm.c:188`,
  `Application/Library/config_store.c:225` (Config_DeviceHidden,
  Config_SetDeviceHidden).
- Problem: a device counts as hidden only when every motor is hidden, and the
  toggle hides or shows every motor.
- Effect: with only M12 hidden the manager shows the device as "Visible";
  Hide then Show makes M12 visible again although the user hid it on purpose.
  There is no "partly hidden" state.
- Fix (proposed): show "Partly hidden" when some motors are hidden, and keep
  the per-motor flags (e.g. remember them, or only hide/show the whole device
  without touching motor flags).

### KI-20: Auto status is a device setting but appears in each motor's settings

- Where: `Application/Library/screen_edss.c:631-634` (isMotorItem,
  ITEM_AUTO_STATUS).
- Problem: the code treats ITEM_AUTO_STATUS as a device setting, but the item
  is listed in the settings of M11, M12, ...
- Effect: changing it on M12 also changes M11, and nothing on screen says so.
- Fix (proposed): label it as a device setting (e.g. "Device auto status") or
  move it to the device manager.

### KI-21: USB BTN reply reports LOCK of module 0 only

- Where: `Application/USB_DEVICE/App/usbd_cdc_if.c:380` (CDC_ButtonCommand,
  device manager reply).
- Problem: `LOCK=` is taken from Config_Motor(id, 0).
- Effect: on a device with several motors the test/USB tools see only M11's
  lock. No effect on the user interface.
- Fix (proposed): report the lock of every motor (e.g. `LOCK=01` per module)
  or say "any motor locked".

### KI-22: Device control should show the device name and the motor name on separate rows

- Type: feature request (2026-11-27).
- Where: `Application/Library/screen_dcs.c:191-199` (title row),
  `Application/Core/Inc/define.h` (DeviceStorageData_t, MotorStorageData_t),
  `Application/Library/screen_edss.c` (ITEM_NAME, captioned "Device name").
- Now: one title row, `" %c%u%u.%.19s"`, e.g. "M12.Default": device type
  letter, device number, module number and the motor name. Only motors have a
  name (`MotorStorageData_t.name[20]`); the editor that changes it is captioned
  "Device name".
- Wanted:
  - Top row: the device, `D<device number>. <device name>`, e.g. "D1. Tu dien 1".
  - Row below: the motor, `M<module number>. <motor name>`, e.g.
    "M1. Motor nuoc". The motor number restarts at 1 on every device
    (M + module index + 1), it is not the old "M12".
  - Both names can be edited by the user.
- Work needed:
  - Add a device name to the stored settings. DeviceStorageData_t is part of
    the packed CFG3 flash layout, so the name must be appended at the end of
    SystemStorageData_t (e.g. `char deviceNames[MAXIMUM_DEVICE][20]`) or the
    format bumped to CFG4 with a conversion; default "Default" or empty.
  - A device name editor (device manager or device settings), and recaption the
    current motor name editor as "Motor name".
  - Rework the control screen layout (128x160): status bar 0..15, title now
    20..37, currents from 47; two rows need about 2 x 14 px, so the rows below
    move down or the RSSI moves into a row.
  - Switch every other screen to the same labels (decided 2026-11-27): `D<n>`
    for a device, `M<n>` for a motor (module index + 1), the type letter
    ('M', 'P', 'E') is dropped from labels (pairing still shows "Type"):

    | Screen | Now | New |
    |---|---|---|
    | Device settings title, screen_edss.c:106 | ` M12.Default-Set.` | `D1 M2. Motor nuoc` (or two rows like the control screen) |
    | Timer title, screen_dcs.c:702 | `M12 Timer` | `D1 M2 Timer` |
    | Lost-link notice, screen_dcs.c:289/304 | `M2` (the device number) | `D2` |
    | Monitor title, screen_dms.c:112 | `M12 Info: Default` | `D1 M2 Info: Motor nuoc` |
    | Device manager row, screen_dvm.c:129-136 | `M1 [Visible] 3 motors` / `M1 [Visible] Default` (motor 1's name) | `D1. Tu dien 1 [Visible] 3 motors` |
    | History picker, screen_hs.c:314-317 | `M1` / `Default` (motor 1's name) | `D1` / `Tu dien 1` |
    | History list title, screen_hs.c:178 | `M1 Default` | `D1. Tu dien 1` |
    | History list row, screen_hs.c:376 | `M12 ON Off > On` | `M2 ON Off > On` |
    | History detail title, screen_hs.c:173 | `M12 Event` | `D1 M2 Event` |

    Today the labels are mixed: some use the type letter, the lost-link notice
    and history hard-code 'M', and the device manager and history show motor 1's
    name where a device name belongs (ScrnCommon_MotorName(id, 0)).
  - Tests that check label text (tools/test_*ui*.py, test_motor_navigation.py,
    test_history_ui.py, test_device_menu.py).

### KI-23: Slave (lora_device) changes needed for alarms

- Type: work in the slave repo `lora_device`, needed by KI-17. Line numbers
  refer to lora_device origin/main fbf5aae.
- Where: `Application/Core/Src/main.c:672-680` (Motor_SendCurrentAlarm),
  `:790-808` (raise/clear/repeat), `Application/Core/Inc/define.h:17-20`
  (alarm codes), `Application/Library/communicationLora.h` (alarm payload).
- Problems found:
  - `sysInfo.alarmCode` is never assigned, so every CMD_ALARM_MESSAGE carries
    code 0 (ALARM_NONE). The only alarm the slave detects is low current
    (HIST_ERROR_LOW_CURRENT), and there is no code for it.
  - `alarmTime` is set to the current time on every 15 s repeat, so the
    remote cannot show when the alarm started.
  - Nothing is sent when the alarm clears; only a HIST_CLEAR history record.
  - `moduleIndex` is always 0 (fine while the slave drives one motor).
- Fix (decided 2026-11-27, with KI-17 option A+C):
  - Add ALARM_LOW_CURRENT and set `alarmCode` before sending; add
    ALARM_CLEARED = 0x80 on both sides (remote Alarm_Name_e too).
  - Keep the alarm start time and send it in every repeat.
  - On clear, send CMD_ALARM_MESSAGE with ALARM_CLEARED for the module,
    three times a few seconds apart (the message has no acknowledgement).
  - Send the real module index once the slave drives several motors.
  - Update the slave before the remote.

### KI-24: Only one remote per network

- Type: feature request (2026-11-27). Design: docs/MULTI_REMOTE.md.
- Problem: pairing a device with a second remote takes it away from the
  first; transaction ids of two remotes collide; Lock is checked on the
  remote only; two remotes with different clocks make the device clock jump.
- Decided: one main remote and secondary remotes paired like devices (type
  'R'); the main remote's configuration is pulled over LoRa by version;
  secondaries can view, switch, set timers and acknowledge alarms; the slave
  enforces Lock, set with the other settings in CMD_SET_CONFIG, which the
  slave then accepts while the motor is ON (Lock does not block timers or
  local buttons); changing the radio settings means pairing everything again.

## Fixed

### KI-9: USB console stops reading until the remote is woken or replugged

- Fixed: 2026-11-27, commit "fix: no STOP mode on external power (KI-9)".
  Root cause inferred from the code; the stall could not be reproduced on
  demand (100 s idle with COM6 closed did not trigger it). Reopen if it
  happens again with this firmware.
- Cause: usbActive() needs SOF within 2 s. When Windows suspends the idle USB
  device (USB selective suspend is enabled on the bench PC), SOF stops, the
  remote goes to SLEEP after IDLE and Power_TryStop() enters STOP. STOP wakes
  only on LoRa DIO, buttons and the RTC (no USB wake-up line), so the host's
  resume and writes go unanswered until a button press.
- How: Power_TryStop() never enters STOP while the charger reports external
  power (CHRG or STDBY active). SLEEP (backlight off, slow scans) still
  applies; on battery STOP is unchanged. Tested in test_power_policy (same
  idle setup: battery enters STOP, charging or full does not).

### KI-12: History events do not record the motor

- Fixed: 2026-11-27, commit "fix: record the motor of each history event (KI-12)".
- How: HistoryEvent.flags bits 1..3 (HIST_MODULE_MASK) carry the module of a
  motor event (ON, OFF, ALARM, CLEAR). The record size, field order and CRC are
  unchanged, and records written before have the bits clear, which reads as
  module 0 (single-motor devices), so stored journals need no conversion. The
  remote accepts modules 0..5 and still rejects other flag bits. The event
  detail title names the motor ("M12 Event"); list rows add "M<d><m>" only for
  devices with several motors. Link events stay device-level ("M1 Event").
  The slave's header defines the same bits; SingleMotor leaves them 0.

### KI-4: Editing a motor without a storageData.motors slot is lost silently

- Fixed: 2026-11-27, commit "fix: small screen issues KI-4..KI-8". Not reachable from the UI since b881d5f
  (the control screen only opens Device setting for a visible motor); fixed
  defensively.
- How: ScrnEdss_CanEdit(item) refuses motor items (threshold, delay, lock,
  hide, name) when the motor has no slot. Device setting gives a long beep
  instead of opening the editor, and the editor refuses to store (long beep,
  no flash write). Delete and auto-status stay editable (device settings).

### KI-5: History picker shows deleted devices as "[Hidden]"

- Fixed: 2026-11-27, commit "fix: small screen issues KI-4..KI-8".
- How: "[Hidden]" only for a device that is not deleted.

### KI-6: Labels hard-code module 1

- Fixed: 2026-11-27, commit "fix: small screen issues KI-4..KI-8".
- How: rule "M<device>" for device-level labels, "M<device><module>" for one
  motor. The connection-lost notice, history title and picker show "M<device>";
  device manager rows show "M<device>" with the motor's name, or "<n> motors"
  when the device has several.

### KI-7: Lock/Hide confirm always rewrites flash

- Fixed: 2026-11-27, commit "fix: small screen issues KI-4..KI-8".
- How: applySelection() returns whether Lock, Hide or Delete changed; the
  configuration is saved only then (both the click and the long-press paths).

### KI-8: Pairing screen redraws twice when the motor pool is full

- Fixed: 2026-11-27, commit "fix: small screen issues KI-4..KI-8".
- How: connect() calls Main_ConnectToDevice() first and draws the screen once
  with CONNECTING or NO_ROOM.

### KI-13: Flash_WriteData is dead code

- Fixed: 2026-11-27, commit "refactor: remove dead Flash_WriteData (KI-13)".
- How: removed from flash.c/flash.h. The linker already dropped it, so the
  application binary is unchanged (CRC32 E80B8C75).

### KI-10: Pairing resets motor settings on the remote but not on the slave

- Fixed: 2026-11-27, Lora_device commit 1ef8f76 (pairing reset) and remote
  commit 41032bc (interim sync removed).
- How: the slave sets threshold, delay and auto-status to 0 when it accepts
  CMD_CONFIG_CONNECTION, in the same flash record as the new radio settings.
  The remote already gives every motor these defaults in storePairedDevice, so
  no CMD_SET_CONFIG is needed. Bench: slave A=2 -> A=0 at CONFIRM, remote AUTO=0.
- History: 926d09a first fixed it on the remote by sending CMD_SET_CONFIG once
  the slave was heard on its new node; replaced because that sync was lost on a
  remote restart, needed retries (motor ON) and extra radio traffic.

### KI-1: Modules other than 0 never receive status or alarms

- Fixed: 2026-11-26, commit 59abdc6 (slave: Lora_device c52194c).
- How: the link monitor probes with moduleIndex 0xFF and the slave answers
  with one record per motor (docs/STATUS_ALL.md); handleGetStatus applies
  each record to `Main_MotorState(devNo, module)`; alarms are accepted for
  modules 0..5. Rather than probing every module, one frame carries all.

### KI-2: Schedule tracking is per device, commands are per motor

- Fixed: 2026-11-26, commit 59abdc6.
- How: `Schedule_CancelForMotor` / `Schedule_GetStateForMotor` take
  (node, module), and `Schedule_HandleStatus` matches the tracked module.
  Covered by tools/test_status_protocol.py.

## Closed (not fixed)

### KI-3: Firmware upgrade wipes all pairings and settings

- Where: `Application/Library/config_store.c:46` (Config_Load); commit 129df91
  removed the CFG2/raw load paths.
- Problem: only "CFG3" slots of size `sizeof(SystemStorageData_t)` are accepted;
  anything else falls back to loadDefaults() and is saved immediately.
- Effect: a unit on CFG2 firmware loses all paired devices and radio settings
  after the update.
- Closed: 2026-11-27, intended (owner decision). There is only one remote and
  it already converted to CFG3 with firmware 55588a5, so the conversion was
  removed on purpose (129df91). Older blocks are not read.

### KI-11: New remote with an old slave reports the link as lost

- Where: `Application/Library/link_monitor.c` sendProbe (moduleIndex 0xFF);
  slaves older than Lora_device c52194c ignore moduleIndex != 0.
- Problem: probes for every module go unanswered by an old slave.
- Effect: after a remote-only update the device shows "NO LINK" although it
  works. Documented in docs/STATUS_ALL.md: update the slave first.
- Closed: 2026-11-27, won't fix (owner decision). Always update the slave
  before the remote.
- Fix (if ever needed): after a probe gets no answer, retry with moduleIndex 0
  and remember that the device needs single-module requests.
