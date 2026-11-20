# Remote UI consistency audit, 2026-11-19 (UTC+7)

## Fixed

- Main menu and pairing created a placeholder 10:30 clock but their renderers
  never wrote the newly formatted RTC text to it. Both now use a live header.
- Date/time editor used a centered HH:MM:SS clock and a custom battery position,
  without the standard icon/unknown-voltage/low-battery handling.
- A single Screen_StatusBarCreate/Update implementation now covers control,
  timer picker/custom editor, menu, monitor, both settings menus/editors, date/time,
  history (picker/list/details), device manager and the empty-control screen.
  Clock HH:MM is left-aligned; bitmap 15x9 starts at (87,3); battery percentage
  occupies (104,0)..(127,15), right-aligned. Header text never scrolls.
- History now uses the same live clock/icon, rather than a separate date-only
  header/raw battery value. Event date/time remains in event details.
- Control battery rendering no longer relies on GIF stop-frame selection,
  which could not select frame zero. The common bitmap path supports empty and
  unknown batteries and animates only when charging. Existing UGUI setters avoid
  redraw when text, color or bitmap pointer has not changed.
- Device manager and empty-control screens now retain a status bar. Manager has
  four rows per page, with disjoint title, rows, help and footer regions; the
  pagination message no longer reuses the battery textbox.
- Monitor columns no longer overlap. Full 32-bit serial, signed RSSI and 1-based
  module number fit their boxes. Last RX uses link uptime instead of editable
  wall time, avoiding underflow after RTC changes. Device-name title formatting
  is bounded, including the device settings editor's former 30-byte overflow risk.
- Pairing created each footer textbox twice, with duplicate IDs; removed the
  redundant initial footer. Its serial/value columns now fit a full 32-bit serial
  and are separate from labels, with gaps between rows.
- General/device editor descriptions and save/cancel help are two bounded lines,
  not long scrolling/clipped single lines. Radio-editor range and status-disabled
  wording describe the actual setting. Only selected history-list/picker rows
  may scroll; details and header remain stationary.

## Verification

Keil USB_Application app: 127872 bytes, CRC32 58C0857D; final incremental build
0 errors/0 warnings. App-only USB update with CRC verification; no bootloader,
slave firmware, settings or history erase.

175 discovered ARM firmware tests passed (7 role-specific skips), including ten
new UI consistency tests. These execute all screen families, all eight General
editors, seven device editors, both timer layouts and all six active pairing
states, checking header geometry/live time, battery 0/unknown, duplicate object
IDs, object count, row bounds, full serial and uptime-based link age.

Native non-mutating UI audit:

```powershell
# Begin on normal control screen; no Save, pairing or motor-power action.
python tools/check_ui_consistency.py --run --capture-dir dist/ui-audit
```

Opened monitor, timer picker/custom, both device-settings pages and all seven
editors, menu/manager, General pages/all eight editors/date-time, and three history
views. Snapshots manually reviewed under dist/ui-audit/. Main menu advances from
00:34 to 00:36 in the captures, not the old 10:30 placeholder; Date/time header
matches the other screens. Pairing states, battery boundaries and all-20-device
pagination were ARM-emulated rather than modifying bench pairing or battery.

CONFIG matched before/after, motor stayed OFF with no timer, all 18 histories
retained. HEAP/MIN_HEAP=1664, STACK_SYS=488, STACK_LCD=712, STACK_RX=600;
LCD_ERR=LCD_REC=LCD_FAULT=RF_SPI_ERR=0 after the native navigation audit.
An additional final app upload only clarified two editor description strings.
This audit does not claim the previously intermittent USB hang is eliminated.
