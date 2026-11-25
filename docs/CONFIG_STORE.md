# Configuration storage (A/B slots) — 2026-11-25

Replaces the single-copy save described in PERSISTENCE.md.

## Layout

The 4 KiB settings area at 0x0803F000 is split into two 2 KiB slots:

| Slot | Address | Content |
| --- | --- | --- |
| A | 0x0803F000 | 16-byte header + SystemStorageData_t (1545 bytes) |
| B | 0x0803F800 | 16-byte header + SystemStorageData_t (1545 bytes) |

Header: `sequence` (uint32), `length` (uint16), `reserved` (uint16, 0xFFFF),
`crc` (CRC-32 of the payload), `magic` (0x33474643, "CFG3").

## Save

`Config_Save()` (systemTask, when `isNeedSaveConfig` is set) writes to the slot
that does not hold the current copy: erase the page, program the payload,
program sequence/length/crc, and program the magic word last. It then reads the
slot back. A failure gives a long beep, as before.

A power loss at any point leaves the other slot untouched. At start-up
`Config_Load()` takes the valid slot (magic, length and CRC correct) with the
highest sequence number; sequence numbers are compared with wrap-around.

## Older formats

Only "CFG3" slots are read. The earlier formats (the raw single-copy block at
0x0803F000 and "CFG2" slots with one set of settings per device) were converted
by firmware 55588a5; that conversion was removed on 2026-11-26 once the only
remote had saved CFG3. A board that still holds only an older format starts
from the defaults and loses its pairings. `lastWriteSuccess` is no longer read
but stays in SystemStorageData_t so the CFG3 layout does not change.

If no slot is valid, the defaults are loaded and saved to slot A immediately.

## General settings have a single copy

netIdConfig, frequencyConfig, sfConfig, sleepMode, tUpdate and tSleepUpdate are
no longer duplicated in SystemInfo_t. The application reads and edits them in
`storageData`; `ScrnCommon_SaveGeneralSettings()` only requests the flash write
and applies the new periods. `Config_Load()` corrects out-of-range values
(net id, frequency, SF, intervals) once at start-up.

## Device settings have a single copy

Device identity (node id, factory info, deleted flag) exists only in
`storageData.DeviceConfig[]`, motor settings only in the motor pool below.
`DeviceInfo_t` and `MotorData_t` keep runtime state (status, currents, timer,
link and RSSI), indexed like `DeviceConfig`. Editors write `storageData`
directly and `ScrnCommon_SaveDeviceSettings()` only requests the flash write.
`deviceBuff` shrinks from 7920 to 3840 bytes.

## Motor settings pool ("CFG3", 2026-11-26)

A device can drive up to `MAXIMUM_MODULE` (6) motors. Name, alarm
threshold/delay, lock and hide are per motor and live in a pool shared by all
devices, `storageData.MotorConfig[MAXIMUM_MOTOR]` (40 slots). Each slot names
its owner, `(device, module)`; `device == MOTOR_SLOT_FREE` (0xFF) marks an
unused slot.

| Part | Bytes |
| --- | --- |
| General settings | 15 |
| `DeviceConfig[20]`: node 1, factoryInfo 8, isDelete 1 | 200 |
| `MotorConfig[40]`: device 1, module 1, name 20, threshold 4, delay 4, lock 1, hide 1 | 1280 |
| `lastWriteSuccess` | 1 |
| Link settings (magic, idle, timeout, autoStatus[20], checksum) | 49 |
| Total | 1545 (slot payload limit 2032) |

- `Config_Motor(device, module)` returns a motor's slot or NULL; nothing else
  should search the pool. Screens use `ScrnCommon_CurrentMotor()`, which
  returns a blank placeholder instead of NULL.
- Pairing gives the device `Config_ModuleCount(numberOfModule)` slots (1..6)
  with the default settings, releasing the slots it held before. Room in the
  device table and the pool is checked before CONFIG_CONNECTION is sent; if
  there is none the pairing screen shows "Memory full" and the device is not
  contacted, so it keeps its old network settings.
- Deleting a device releases its slots.
- A device is hidden when all its motors are hidden (`Config_DeviceHidden`);
  the device manager hides or shows all of them. Device setting edits the
  selected motor (`systemInfo.currentModuleId`, still always 0 in the UI).
- `Config_Load()` repairs the pool: slots of unknown or deleted devices, bad
  module numbers and duplicates are released, flags are normalised, empty
  names become "Default", and a paired device without a slot for module 0
  gets one if the pool allows.

Not done yet: the UI cannot select a module other than 0, the remote ignores
status replies for modules other than 0, and the slave firmware supports one
motor only.

## Verification

Host tests (gcc, flash mocked at the real addresses) covered blank flash,
alternating saves, a power loss before every one of the 227 flash operations of
a save, older formats being ignored, two corrupt slots, sequence wrap-around
and sanitising. The ARM emulation tests
in tools/ were updated for the new SystemInfo_t offsets but must be run against
a fresh Keil build.

CFG3 (2026-11-26): tools/test_config_load.py runs Config_Load/Config_Save of
the built firmware in Unicorn with emulated flash: blank flash, alternating
saves, older formats ignored, pool repair and an interrupted save. tools/test_pairing.py covers allocation,
re-pairing with fewer modules, and refusing a full device table or pool before
the device is contacted. tools/config_store_host/test_config.c was updated for
the new layout but has not been run (needs Linux and gcc). Bench: the remote
converted its CFG2 block on first boot (firmware 55588a5) with all settings and
link settings unchanged, then wrote and reloaded CFG3 twice (hide toggle across
resets); the UI audit passed.
