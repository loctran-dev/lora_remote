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
