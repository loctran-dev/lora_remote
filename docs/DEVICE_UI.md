# Device settings layout - 2026-11-09

Remote application installed via COM6: 111028 bytes, CRC32 9E48655C verified.
No bootloader or slave application was programmed in this update.

Device settings now shares General settings' four-row, two-line layout:

- Full-width labels; cyan, right-aligned values below each label.
- Both lines highlight the selected item. Page indicator 1/2 or 2/2.
- Page 1: Low-current alarm, Alarm delay, Lock, Hide.
- Page 2: Delete device, Device name, Auto status interval; fourth row hidden.
- Existing item/editor IDs 0..6 and stored field meanings are unchanged.
- Editor save/cancel retains selection and page. A fresh entry from Control
  resets selection to the first item. Hide/delete still follow their original
  control-screen redirection rules when actually confirmed.
- Fixed values do not scroll. The 19-character maximum name fits the full width.

The patch only changes menu rendering/navigation. It does not alter settings,
LoRa configuration, safety rules, motor outputs or Flash layout.

Verification:

- 65 remote host/ARM-code tests pass. New Device tests cover all seven editor
  IDs, actual cancel return paths, unchanged storage/device data, selection
  bounds, page contents, long names and field formatting. Shared layout tests
  cover hiding/restoring the unused fourth row and the two-page indicator.
- Hardware: opened all seven editors, canceled without edits, except saved the
  existing unchanged name. Lock/Hide/Delete were not confirmed or changed.
- Tested editor return on page 2, fresh Control entry and repeated page flips.
- LCD readback and USB stayed responsive. STATUS reception remained healthy.
- Slave STATUS remained PWR=0. No motor ON/OFF command was used.

Screenshots in dist/: device-ui-page1.bmp, device-ui-page2.bmp,
device-ui-after-editor.bmp, device-ui-back-page1.bmp.

Configuration before/after identical: SLEEP=1, UPDATE=5, SLEEP_UPDATE=5, IDLE=45,
LOST=180, AUTO(device 1)=2, NET=1, FREQ=433, SF=7. Device 1 still showed Default,
alarm 0.00 A, delay 0 s, Lock=No and Hide=No.
