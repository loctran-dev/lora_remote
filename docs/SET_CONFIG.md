# LoRa motor configuration command

Implemented in FIRMWARE/Lora_remote/Application for the remote and
FIRMWARE/Lora_device/Application for SingleMotor.
Original non-bootloader application trees are unchanged.

Opcode: 0x08 (CMD_SET_CONFIG). Uses the existing LoRa framing and CRC.
Request payload is packed, little-endian, 7 bytes. Slave also accepts legacy
5-byte requests, preserving its current auto status period in that case:

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | uint8 | Module index; SingleMotor accepts only 0 |
| 1 | uint16 | Alarm threshold in 0.01 A, range 0..9999 |
| 3 | uint16 | Alarm delay in seconds, range 0..9999 |
| 5 | uint16 | Auto status period, range 0..9999 seconds; 0 disables |

Example: module 0, 12.34 A, 30 seconds, status every 60 seconds =
00 D2 04 1E 00 3C 00. See LINK_SLEEP.md for the current workflow/tests.

The slave accepts only requests addressed to its current network and node.
Wrong payload length, packet type, or address is ignored. Unsupported module
or out-of-range values return result 0 without changing either setting.
Valid values are assigned to sysInfo.alarmThresholdCentiAmp and
sysInfo.alarmDelaySeconds, committed to slave settings Flash, and return result 1.
Flash failure or MOTOR_POWER_ON returns result 0 and rolls back the RAM edit.

Response: existing packed PayloadCommonResponse_t (moduleIndex, result),
with the request transaction ID retained.

Holding Center to save I alarm, Alarm delay or Auto status sends all three values.
Remote matches the ACK by network, node, transaction and payload size.
Success gives two short beeps; rejection or local queue failure gives a long beep.
There is no automatic retry or ACK timeout indication yet. UI values are local
edits, not proof of slave acknowledgement.

Remote edits are now also saved to its local settings Flash. Saving the local
intent is independent of receiving a slave ACK; there is still no automatic
retry/resynchronization. Slave USB MODE reports I (0.01 A) and D (seconds).
Both boards restore their stored values on startup. See PERSISTENCE.md.
The slave now consumes these values in its existing LOW-current warning (any
phase below I alarm for Alarm delay). Threshold zero disables the warning.
It is not overcurrent protection and does not automatically stop the motor.
See ../../Lora_device/docs/CURRENT_ALARM.md for the subsequent integration/tests.

Hardware checks still required:

- Save threshold and delay, verify payload, transaction and successful ACK.
- Invalid module, values >9999, truncated/oversized payload must not change RAM.
- Wrong address/type/CRC must not change RAM.
- Repeat a valid request: same RAM result, no motor ON/OFF side effect.
- Drop ACK: no false success beep; local UI still shows the edited value.
