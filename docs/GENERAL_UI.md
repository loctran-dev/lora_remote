# General settings layout - 2026-11-09

Application deployed over COM6, CRC32 verified: 2114A083, 111020 bytes.
Bootloader and slave application were not changed in this UI update.

## Layout

General settings now uses four two-line rows per page. Each label gets the full
122-pixel content width; its value is on a separate, right-aligned cyan line.
The selected item highlights both lines. Header shows 1/2 or 2/2. Values and
page counters do not scroll; only the selected label may scroll if necessary.
The existing default-font labels all fit without scrolling.

Page 1 groups Sleep enable, Sleep after idle, Sleep check interval and Link
timeout. Page 2 groups Link scan interval, LoRa network ID, frequency and SF.
Only the display order changed. Original editor IDs and stored field meanings
are preserved by the mapping {0,6,2,7,1,3,4,5}.

Saving/canceling an editor retains selection and page. Entering General settings
from the main menu starts at the first item. The subsequent DEVICE_UI.md update
also applies this shared stacked layout to Device settings.
Page changes clear the entire two-line body, avoiding stale labels or values.
No link supervision, sleep protection or motor-control policy was changed.

## Verification

- 59 remote host/ARM-code tests pass, including nine new menu/layout tests.
- Built ARM tests verify editor mapping, selection retention, navigation bounds,
  non-overlapping rectangles, label widths, counters, fixed values, preservation
  of stored settings and unchanged Device-menu geometry.
- Hardware: opened all eight editors, canceled seven, saved the unchanged idle
  value in the eighth; configuration remained identical before/after.
- Repeated navigation across both pages and back did not leave stale rows.
- USB remained responsive after LCD readback; STATUS reception continued.
- Motor stayed PWR=0; no motor ON/OFF commands were used.

Screenshots in dist/: general-ui-before.bmp, general-ui-after-page1.bmp,
general-ui-after-page2.bmp, general-ui-after-cancel.bmp, general-ui-after-back.bmp.

Configuration preserved from the user's current board (not reset to test
defaults): SLEEP=1, UPDATE=5, SLEEP_UPDATE=5, IDLE=45, LOST=180, AUTO(device 1)=2,
NET=1, FREQ=433, SF=7. Earlier LINK_SLEEP.md bench-state notes are historical.
