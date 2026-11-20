# Timer UI refresh - 2026-11-17

Remote application only. Slave firmware, radio protocol and bootloaders unchanged.

## Timer selection and editing

- Dedicated picker with four rows per page, page indicator and one highlighted row.
  Custom time is initially selected; Disable timer and the existing 30-minute presets
  remain available.
- Custom duration is HH:MM with four independent digit textboxes. CENTER moves the
  yellow highlight through hour tens, hour units, minute tens, minute units, then
  back to hour tens. The colon is never selectable.
- LEFT increases the highlighted digit; RIGHT decreases it. Minute tens wraps at
  5, other digits at 9. Supported duration: 00:01 through 99:59.
- Hold CENTER in the custom editor to apply. 00:00 is rejected with a visible
  minimum-duration message, without transmitting a command.
- Hold RIGHT to cancel/back without transmitting. Hold CENTER in the preset
  picker also returns without submitting the highlighted item. Click CENTER
  on a non-custom preset to submit it.
- Timer browsing/editing prevents idle sleep. A connection-warning event does
  not change the selected device or discard the draft while editing.

## Main control screen

The countdown is a large cyan HH:MM:SS readout. A separate small header shows
Auto stop on the left and the deadline At HH:MM on the right. The old clock
icon and mixed duration/deadline string are removed.

A timer appears only when the latest motor state is ON and STATUS contains a
nonzero deadline. OFF hides the countdown even if the slave reports an old
deadline. No timer shows Not set. Expiry clamps to 00:00:00 while waiting for
confirmed OFF; it does not optimistically change motor state. Offline data is
marked Last timer / Offline in orange. Text/color setters retain their existing
change-detection behavior, avoiding redraws when content is unchanged.

## Verification

The entries below describe the initial UI-only build BAA4469C. The subsequently
flashed confirmation/retry build 5F20D2C3 has passed actual countdown and auto-OFF
testing; see [SCHEDULE_CONFIRMATION.md](SCHEDULE_CONFIRMATION.md).

- Keil USB_Application build: 0 errors, 0 warnings.
- dist/application.bin: 119904 bytes, CRC32 BAA4469C.
- 96 regression tests pass, including 16 ARM timer-UI tests and the added sleep
  guard test. Six explicit schedule-command checks pass.
- Tests cover four-digit focus, digit isolation, minute/hour wrapping, zero
  rejection, cancellation without transmission, geometry, countdown formatting,
  OFF/expired/offline states and preservation of an editor during a link warning.
- After the user's reset, COM6 responded again. Flashed REMOTE APP ONLY over USB;
  updater verified CRC32 BAA4469C. Neither bootloader nor slave was programmed.
- Native LCD captures confirm the picker and custom editor layout, yellow focus
  on each of the four digits and wrapping back to the first, correct per-digit
  changes, and the zero-duration warning. Cancel returns to the main control UI.
- Navigated past the upper bound (102 RIGHT clicks): selection stops on page
  26/26 at 49 h 30 min. Returning to page 25/26 restores all four rows. Hold
  CENTER cancels the picker without applying a preset. Final screen CONTROL,
  slave STATUS PWR=2, CONFIG unchanged, LCD_ERR/LCD_REC/LCD_FAULT/RF_SPI_ERR all 0.
- Saved captures: dist/timer-ui-main-off.bmp, timer-ui-picker.bmp,
  timer-ui-digit-0.bmp through timer-ui-digit-3.bmp, timer-ui-digit-wrap.bmp,
  timer-ui-zero-error.bmp and timer-ui-last-page.bmp (all in dist/).
- Initial UI inspection did not send ON. A subsequent user-approved no-load
  one-minute physical test FAILED: ON worked but the display stayed Not set and
  the slave did not auto-stop by 65.2 seconds. Cleanup confirmed OFF. See
  [SCHEDULE_TEST.md](SCHEDULE_TEST.md) for evidence and unresolved cause; ARM-only
  rendering tests are not evidence that the live timer command was accepted.

Remote APP ONLY update command:

```powershell
python tools/usb_update.py dist/application.bin --port COM6 --request
```

Picker/custom-digit screens were inspected and canceled with motor OFF. Running
tools/check_schedule_hardware.py --allow-on requires an explicitly approved
no-load setup. The user has since authorized repeated no-load testing by default;
see SCHEDULE_CONFIRMATION.md. The script always attempts OFF cleanup. Preserve
all paired devices and the existing CONFIG values.
