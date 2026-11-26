# CMD_GET_STATUS for every motor — 2026-11-26

A device can drive several motors. `CMD_GET_STATUS` (0x01) now asks for one
motor or for all of them; the slave firmware (Lora_device) implements the same
layout in `communicationLora.h`.

## Request

`PayloadRequestGetStatus_t`, unchanged: `moduleIndex` (uint8), `syncTime`
(uint32).

| moduleIndex | Meaning | Reply |
| --- | --- | --- |
| 0 .. 5 | One motor | `PayloadResponseGetStatus_t` (17 bytes), as before |
| 0xFF (`LORA_MODULE_ALL`) | Every motor of the device | `PayloadResponseGetStatusAll_t` below |

The SingleMotor slave answers 0 and 0xFF and ignores other values.

## Reply for every motor

Packed, little-endian; the device sends only the motors it has.

| Offset | Type | Field |
| --- | --- | --- |
| 0 | uint8 | `moduleIndex` = 0xFF |
| 1 | uint32 | `localTime` (device Unix time) |
| 5 | uint8 | `count`, 1 .. 6 (`LORA_STATUS_RECORDS_MAX`) |
| 6 + 13·i | record | `PayloadStatusRecord_t` i |

`PayloadStatusRecord_t` (13 bytes): `moduleIndex` (uint8), `result` (uint8,
1 = valid), `powerState` (uint8), `phaseCurrent[3]` (uint16, 0.01 A),
`scheduleTime` (uint32, 0 = none).

Six motors take 84 payload bytes, one frame. The slave's autonomous reports
(transaction 0) use this layout too.

## Remote

- The link monitor probes with 0xFF, so one exchange refreshes every motor.
- `handleGetStatus` accepts both layouts. Each valid record updates
  `Main_MotorState(device, module)` when that motor has a pool slot; others are
  skipped. Device-level fields (time, RSSI, link) are updated once per frame.
- `Comm_ParsePayloadResGetStatusAll` rejects count 0, count > 6 and a payload
  whose length is not 6 + 13·count.
- Alarms (`CMD_ALARM_MESSAGE`) are accepted for modules 0 .. 5.
- Schedule tracking is per motor: `Schedule_CancelForMotor`,
  `Schedule_GetStateForMotor`, and status confirmation matches the module.

## Compatibility

- New slave, old remote: replies to module 0 are unchanged, but the old remote
  cannot parse the new autonomous reports and only sees probe replies.
- Old slave, new remote: the old slave ignores 0xFF, so probes go unanswered
  and the link is reported lost. Update the slave first.

## Verification

Remote: tools/test_status_protocol.py (all/single layouts, missing slots,
malformed frames, alarms per module, per-motor schedule tracking) and
test_link_monitor (probe asks for 0xFF). Slave: test_status_clock (0xFF reply
layout, clock sync still applied), test_auto_status (autonomous layout).
Bench: slave 93385914 then remote 54EA5471; the boot probe with 0xFF was
answered (REQUESTS=1 OK=1 FAIL=0), autonomous reports kept STATUS_AGE at
0..2 s, PWR matched the slave, UI audit passed.
