# Hide deployment and hardware tests — 2026-11-08

Remote COM6 serial 5CDB73383837; slave COM7 serial 6D81537A5582.
Final application: 106572 bytes, CRC32 3B2420D0, deployed through USB app update.
Bootloader and slave firmware unchanged. Final incremental build: 0 errors,
0 warnings. 17 ARM-emulation tests and 8 package/update tests passed.

## Observed on hardware

- Saving Hide=Yes for M11 switched the control screen to M21 without renumbering.
- Device manager listed the hidden M11 and allowed restoring it.
- The actual paired list contained eight entries, DEV=1..8, across two pages.
  Initial LCD stale-row artifacts made the last page appear to contain an extra
  entry. Added page counts, explicit empty-row hiding, and a one-time list-region
  clear/redraw on page changes. Final LCD screenshot correctly shows 6-8/8.
- USB BTN replies in Device manager now include DEV, HIDE and LOCK, reporting
  actual selected RAM state after the action rather than relying on LCD text.
- On the final image, each DEV=1..8 was set to HIDE=1 with an assertion on its USB
  reply. The control screen showed "No visible devices".
- Reset through BOOTLOADER then INFO/RUN (no reprogramming). The empty screen
  persisted. Entered manager directly using Center from the empty screen; every
  DEV=1..8 still reported HIDE=1 and was individually restored to HIDE=0.
- Lock=Yes for M11 also survived an earlier reset during this deployment session,
  verified on the device settings LCD after showing M11 again. Then restored No.
- Tested ON/OFF LONG while all entries were confirmed hidden, on the initial
  deployment image. Slave stayed PWR=0. No unlocked ON command was sent.
- Final slave STATUS: PWR=0 A_MA=0 B_MA=0 C_MA=0. SWD HotPlug reads of slave
  startTime/stopTime at 0x200006DC remained 0/0.

Final state: all eight entries Visible, all LOCK=0, main screen M11.
The slave remained OFF throughout. This used USB-injected UI events and physical
LCD readback, not manually pressed buttons. Cold power cycling and interrupted
Flash writes were not tested; resets do not establish power-failure atomicity.

## LCD evidence (project-root-relative)

- dist/hide-all-hidden.bmp: initial Hide M11 selects M21 (historical filename).
- dist/hide-restored-lock-persisted.bmp: Lock=Yes retained after reset.
- dist/hide-final-manager-page2.bmp: corrected page 6-8/8, all Visible.
- dist/hide-final-no-visible.bmp: all hidden on final image.
- dist/hide-final-after-restart.bmp: all hidden retained after reset.
- dist/hide-final-restored.bmp: restored control screen M11.

tools/capture_screen.py now waits up to 15 seconds for COM to reconnect after
reset, rather than failing immediately during USB enumeration.
