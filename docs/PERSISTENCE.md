# Settings persistence — 2026-11-08

Current sleep/link extensions and migration are documented in LINK_SLEEP.md;
the disabled-sleep and 836-byte-only statements below describe the older build.

Implemented in the requested order: alarm settings on both boards, remote
name/deletion, LoRa configuration, then general settings.

## Saved settings

| Setting | Remote | Slave |
| --- | --- | --- |
| Alarm threshold and delay | Local settings plus SET_CONFIG request | Journal commit before success ACK |
| Device name, deleted flag | Local settings | Not applicable |
| Hide / Lock | Local settings (existing behavior retained) | Not applicable |
| LoRa net ID, frequency, SF | Saved from General settings | Saved when CONFIG_CONNECTION is accepted |
| Node ID | Pairing table | Saved with connection configuration |
| Sleep enable, normal/sleep intervals | Local settings | Not applicable |

Remote keeps the existing 836-byte packed SystemStorageData_t layout at
0x0803F000, using only module 0 as supported by the current persistent device
layout. Save paths now copy edited values into storageData before the existing
systemTask Flash write. Readback failure produces a long beep.

Startup validates the settings marker, clears invalid initial data, sanitizes
radio settings, and restores general settings. Invalid/zero interval values from
older firmware fall back to 5 s normal / 300 s sleep. New UI interval entries
must be 1..9999 seconds. Net ID 0/255 and frequency below 137 are rejected.

Normal polling uses tUpdate (including changes made without a reboot).
Sleep enable and sleep interval are persisted only: STOP/sleep execution remains
disabled. This work does not enable low-power behavior or change motor protection.

LoRa startup now initializes the radio using stored frequency/SF, rather than
always 433/SF7. Editing radio settings saves a pending configuration; the active
radio is not automatically migrated along with all existing slave devices.
Restart to apply it on remote; slave retains its existing configuration/confirm
workflow. Both endpoints must use matching settings. Do not assume changing one
screen reconfigures every paired slave.

Slave journal layout, Flash-full behavior and limitations are documented in
../../Lora_device/docs/PERSISTENCE.md. Bootloader layouts/protocols are unchanged.

## Verification and deployment

Final installed application images (app only; no bootloader programming):

| Board | Application bytes | CRC32 |
| --- | --- | --- |
| Remote COM6 | 107244 | EBD98F77 |
| Slave COM7 | 39636 | 5BDEFC73 |

Both builds passed with zero errors; existing warnings remain (remote's last
compile log: 8; slave's last compile log: 1 missing newline in button.h).
Tests passed: 19 remote ARM-emulation tests, 10 slave persistence/handler tests,
8 package/update tests per board, plus 70 LoRa LDO arithmetic-equivalence cases.
The emulation tests execute the built Thumb code; peripheral/HAL calls are mocked.

Physical-board tests used USB-injected UI events and LCD/USB readback:

- Set alarm to 0.01 A / 1 s from the remote; confirmed slave MODE I=1 D=1.
  Reset slave and remote independently; both retained these values.
- Rename Default -> Eefault; confirmed the edited name in the rename editor
  after reset; restored Default.
- Save SF8 on remote, reset, confirm CONFIG SF=8 and ACTIVE_SF=8. Saved/restored
  SF7 and confirmed ACTIVE_SF=7 after reset.
- Set Sleep=Yes, tUpdate=6 and tSleepUpdate=301; reset and confirm all three.
  The actual timer-start arguments for a 6-second interval were also checked
  in ARM emulation (6000 ticks at the project tick rate).
- Restored alarm 0.00 A / 0 s on both boards; reset slave and confirmed I=0 D=0.
  Restored Sleep=No, intervals 5/300 s, net=1/frequency=433/SF7. Remote reset
  confirmed those settings; final control screen is M11, name Default.

No real paired device was deleted during hardware testing. Delete staging,
filtering and settings bytes were checked in emulation. Slave pairing with
non-default net/node/frequency/SF, successful persistent ACK and Flash-failure
NACK/rollback were checked in emulation, not by re-pairing the live motor board.
No motor ON command was sent; final slave STATUS PWR=0, currents all zero.
Manual button presses, interrupted writes and cold power cycling were not tested.

Evidence: dist/persist-alarm-before-reset.bmp, persist-alarm-after-reset.bmp,
persist-name-editor-after-reset.bmp, persist-radio-sf8-after-reset.bmp,
persist-general-before-reset.bmp, persist-general-after-reset.bmp,
persistence-final-device-settings.bmp and persistence-final-control.bmp.

## Read-only USB diagnostics

- Remote CONFIG: saved-intent RAM values and active radio values.
- Slave MODE: state/net/node plus I, D, F and SF.
- Slave STATUS: motor/current state.

Settings must be saved through the normal UI/LoRa handlers; these diagnostic
commands do not write settings. Local remote edits are saved even if the slave
is unreachable; a local save is not proof of slave ACK or synchronization.
