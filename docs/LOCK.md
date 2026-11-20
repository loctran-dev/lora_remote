# Remote control lock

The existing per-module `isLock` setting now gates `Main_SetMotorPower()`.
When locked, neither ON nor OFF is queued for LoRa transmission. A long beep
indicates the rejected action. The control-screen header also displays
`ON BLOCKED` / `OFF BLOCKED` and `LOCKED` on a red background for 3 seconds.
Another blocked press restarts the display interval; leaving the screen or
unlocking clears it. Physical keys and USB BTN injection use this
same function. Unlocking restores the existing ON/OFF path. Requests address
the selected module rather than hard-coded module 0.

This is a local UI/control guard, not a slave-side authorization mechanism.
Status polling and configuration remain available. Setting/cancelling a timer
and previously scheduled actions are unchanged. Lock and Hide are now persisted
through the existing settings Flash block when saved. The current storage layout
records these flags for module 0 (the single-motor/device UI used here).

Validation: Keil application build passed (0 errors; 21 warnings); eight
existing package/update host tests passed. These tests do not exercise Lock.
Hardware results (2026-11-08): app CRC 2D299356 flashed via COM6, bootloader
unchanged. Confirmed Lock=Yes on LCD before injecting ON LONG and OFF LONG.
Both red messages were captured from the physical LCD and the normal header
returned after waiting beyond the notification interval (screen capture itself
freezes UI updates). Slave COM7 remained PWR=0. SWD HOTPLUG reads of slave
startTime/stopTime at 0x200006DC remained 0/0 before and after both attempts.
Lock was restored to No; no unlocked ON/OFF command was sent. During restoration,
the reset menu selection briefly caused an alarm-threshold edit; it was corrected
back to the original remote value 0.00 A before finishing. Final threshold/delay
were synchronized to the restored remote values. This was not a motor-protection
test. Physical buttons and audio were not independently checked.

LCD evidence in ../dist/: lock-enabled.bmp, lock-on-blocked.bmp,
lock-off-blocked.bmp, lock-notice-cleared.bmp, lock-restored-no.bmp.

Further checks:

- Lock=Yes: ON LONG and OFF LONG must not transmit CONTROL_POWER.
- Repeat with physical keys and USB BTN commands.
- Lock=No: normal ON/OFF path resumes (use a safe motor-disconnected setup).
- Status polling, configuration and other devices/modules remain unaffected.
