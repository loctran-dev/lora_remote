# Device visibility and manager

Source: Lora_remote/Application. This change is local to the remote; it does not
require new slave commands or change motor power, schedules, or alarm reception.

## Usage

- Device settings -> Hide -> Yes -> Center (click or hold) saves and returns to
  the control screen, selecting the next visible device; if none follows, the
  nearest preceding visible device is selected. IDs are never renumbered.
- Previous/Next skip hidden and deleted entries.
- Main menu -> Device manager lists all paired, non-deleted devices, including
  hidden devices. Left/Right select a row; Center toggles Hide/Show. Only the
  selected row scrolls. There are five rows per page. Hold Center/Left/Right to
  return to the menu. The manager's ON/OFF callbacks do nothing.
- When no device is visible, the control screen shows "No visible devices".
  Center or hold Right opens Device manager; hold Center opens the main menu.
  ON/OFF and schedule requests are guarded in the common send functions.
- Hide/Lock are saved to the existing Flash settings block by systemTask.
  Wait at least one system-task cycle before resetting. The storage layout is
  unchanged and represents device/module 0, as used by SingleMotor.

Unhiding does not unlock the device. Hiding is not a security feature and does
not stop a running motor or cancel an existing schedule. Alarm packet reception
is not filtered by Hide; this change does not add any new alarm notification UI.
The current status poller still polls only the selected visible device, not all
hidden devices in the background.

## Implementation notes

Shared selection and manager UI live in Library/screens.c; Device manager is
wired to the previously inactive first main-menu entry. The existing Flash writer
was fixed to interpret its length as bytes (callers pass sizeof), pad partial
words, calculate erased pages, and relock Flash after an error. No new Flash
format, migration, or power-fail-atomic storage scheme is introduced.

## Verification

Keil application build passed. Existing eight package/update host tests passed.
tools/test_visibility.py runs the built Thumb functions in Unicorn with simulated
RAM. It checks selection/filtering/bounds, hidden send guards, manager restoration,
flag persistence staging, and Flash writer calls with mocked HAL functions. This
is not an LCD or physical Flash test. Install Unicorn on the PC if needed:

```powershell
python -m pip install unicorn
python tools/test_visibility.py
```

Run from the project root after building and packaging. Hardware deployment and
UI/readback results are recorded in [HIDE_TEST.md](HIDE_TEST.md). Do not enable
a motor merely to test visibility.
