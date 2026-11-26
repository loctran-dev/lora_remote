# Known issues

Open problems found in the code but not yet fixed. Read this before changing
motor/module handling, schedules, config storage or the screens listed below.

Source: code review of commits 467cf2c..b881d5f (2026-11-26, UTC+7), which moved
the remote from one motor per device to a shared motor pool (CFG3) and made the
control screen step through motors. Line numbers refer to b881d5f and may drift.

When an issue is fixed, move it to "Fixed" with the date and commit. Add new
issues under "Open" with the same fields.

## Open

### KI-3: Firmware upgrade wipes all pairings and settings

- Where: `Application/Library/config_store.c:46` (Config_Load); commit 129df91
  removed the CFG2/raw load paths.
- Problem: only "CFG3" slots of size `sizeof(SystemStorageData_t)` are accepted;
  anything else falls back to loadDefaults() and is saved immediately.
- Effect: a unit on CFG2 firmware loses all paired devices and radio settings
  after the update.
- Status: waiting for owner decision. Is this one-time wipe intended? If not,
  add a CFG2 -> CFG3 conversion (move each DeviceConfig's motor fields into
  storageData.motors module 0) before the defaults fallback.

### KI-4: Editing a motor without a storageData.motors slot is lost silently

- Where: `Application/Library/screen_edss.c:1615`; ScrnCommon_CurrentMotor().
- Problem: when Config_Motor() returns NULL, a static placeholder (re-zeroed on
  each call) is edited; the screen still sets configSavePending.
- Effect: e.g. Lock=Yes is not stored, flash is rewritten, motor stays unlocked.
- Fix: refuse to open/commit the editor when Config_Motor() is NULL (long beep).

### KI-5: History picker shows deleted devices as "[Hidden]"

- Where: `Application/Library/screen_hs.c:303`.
- Problem: Config_DeviceHidden() returns TRUE for a device with no motor slots,
  and deleting a device frees all its slots (Config_FreeMotors).
- Effect: picker shows "M31 [Hidden]" / "Removed device" instead of "M31".
- Fix: test deleted before Config_DeviceHidden, or make Config_DeviceHidden
  return FALSE for deleted devices.

### KI-6: Labels hard-code module 1

- Where: `Application/Library/screen_dcs.c:304` ("M%u1" in connection-lost
  notice); `Application/Library/screen_dvm.c:1461` ("%c%u1" with
  ScrnCommon_MotorName(id, 0)).
- Effect: on M23 a lost link is reported as "M21"; device manager names the
  device after module 0.
- Fix: print the module actually shown, or drop the module digit from
  device-level labels.

### KI-7: Lock/Hide confirm always rewrites flash

- Where: `Application/Library/screen_edss.c:1603` (ScrnCommon_SaveDeviceSettings).
- Problem: configSavePending is set on every confirm; the old
  ScrnCommon_SaveDeviceFlags only set it when flags changed.
- Effect: unnecessary 2 KiB page erase/program cycles.
- Fix: set configSavePending only when the value changed.

### KI-8: Pairing screen redraws twice when the motor pool is full

- Where: `Application/Library/screen_dps.c:1365` (connect()).
- Problem: restart(PAIRING_STATE_CONNECTING) runs before Main_ConnectToDevice()
  checks for room.
- Effect: "Connecting" flashes before "Memory full".
- Fix: call Main_ConnectToDevice() (or check pairingSlot) first, then restart()
  once with the resulting state.

### KI-9: USB console stops reading until the remote is woken or replugged

- Where: USB CDC (`USB_DEVICE/App/usbd_cdc_if.c`) together with the power
  policy (`power_sleep.c`); not investigated.
- Problem: COM6 stays enumerated but every host write times out; the
  application does not read the endpoint. Seen twice on 2026-11-26 (firmware
  675906AE and an earlier build) with SLEEP=1, IDLE=60. Pressing a button on
  the remote or replugging USB restores it. HISTORY_NATIVE.md already notes an
  intermittent "remote USB hang".
- Effect: USB updates (`usb_update.py --request`) and bench tools fail until
  someone touches the remote.
- Fix: unknown; first check whether STOP mode is entered while USB is attached
  (SOF detection) and whether the CDC receive is re-armed after wake-up.

### KI-10: Pairing resets motor settings on the remote but not on the slave

- Where: `Application/Core/Src/main.c` storePairedDevice (Config_AllocateMotors,
  Link_SetAutoStatus(slot, 0)); slave handleConfigConnection.
- Problem: pairing gives every motor the default settings (threshold 0,
  delay 0, auto-status 0) on the remote only. CMD_CONFIG_CONNECTION sets only
  net/node/frequency/SF on the slave, which keeps its I, D and A.
- Effect: remote shows I=0/D=0/AUTO=0 while the slave still applies its old
  alarm threshold, delay and auto-status period (seen: A=2 vs AUTO=0) until
  the user saves the settings again.
- Fix: reset I/D/A on the slave in handleConfigConnection, or have the remote
  send CMD_SET_CONFIG once the paired device is reachable on its new node.

### KI-11: New remote with an old slave reports the link as lost

- Where: `Application/Library/link_monitor.c` sendProbe (moduleIndex 0xFF);
  slaves older than Lora_device c52194c ignore moduleIndex != 0.
- Problem: probes for every module go unanswered by an old slave.
- Effect: after a remote-only update the device shows "NO LINK" although it
  works. Documented in docs/STATUS_ALL.md: update the slave first.
- Fix (optional): after a probe gets no answer, retry with moduleIndex 0 and
  remember that the device needs single-module requests.

### KI-12: History events do not record the motor

- Where: `HistoryEvent` (history_journal.h) on both remote and slave.
- Problem: an event has serial, action, source, before/after, but no module.
- Effect: with several motors, "OFF" in the history cannot be attributed to
  M11 or M12; history titles print "M%u1" (`screen_hs.c:168`, `:303`).
- Fix: add a module field to the journal format on both firmwares, with a
  migration for stored journals.

### KI-13: Flash_WriteData is dead code

- Where: `Application/Library/flash.c:74`, `flash.h:17`.
- Problem: no caller since config_store uses Flash_Erase/Flash_Program; the
  linker drops it.
- Fix: remove it from flash.c/flash.h.

### KI-14: config_store host test has not been run since CFG3

- Where: `tools/config_store_host/test_config.c` (`tools/test_config_store.py`).
- Problem: it needs Linux and gcc; on this Windows bench it is skipped. It was
  edited for the CFG3 layout and the removal of the old formats but never
  compiled or run.
- Effect: the power-loss-at-every-flash-operation coverage is unverified for
  CFG3 (the ARM tests in tools/test_config_load.py cover one interrupted save).
- Fix: run it on a Linux machine or in WSL with a distribution installed.

## Fixed

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

