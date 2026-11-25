# Configuration storage (A/B slots) — 2026-11-25

Replaces the single-copy save described in PERSISTENCE.md.

## Layout

The 4 KiB settings area at 0x0803F000 is split into two 2 KiB slots:

| Slot | Address | Content |
| --- | --- | --- |
| A | 0x0803F000 | 16-byte header + SystemStorageData_t (885 bytes) |
| B | 0x0803F800 | 16-byte header + SystemStorageData_t (885 bytes) |

Header: `sequence` (uint32), `length` (uint16), `reserved` (uint16, 0xFFFF),
`crc` (CRC-32 of the payload), `magic` (0x32474643, "CFG2").

## Save

`Config_Save()` (systemTask, when `isNeedSaveConfig` is set) writes to the slot
that does not hold the current copy: erase the page, program the payload,
program sequence/length/crc, and program the magic word last. It then reads the
slot back. A failure gives a long beep, as before.

A power loss at any point leaves the other slot untouched. At start-up
`Config_Load()` takes the valid slot (magic, length and CRC correct) with the
highest sequence number; sequence numbers are compared with wrap-around.

## Migration from the old format

Boards flashed with earlier firmware hold a raw SystemStorageData_t at
0x0803F000 with `lastWriteSuccess == TRUE` and no slot magic. `Config_Load()`
reads it as before. The first save goes to slot B, so the old copy in slot A
stays readable until slot B is committed; the save after that reuses slot A.
Nothing has to be done by hand. Going back to older firmware after a save to
slot B is not supported: it would read slot A only.

If neither slot nor the old format is valid, the defaults are loaded and saved
to slot A immediately, as before.

## Verification

Host tests (gcc, flash mocked at the real addresses) covered blank flash,
alternating saves, a power loss before every one of the 227 flash operations of
a save, migration from the old format (including an interrupted first save),
two corrupt slots, sequence wrap-around and sanitising. The ARM emulation tests
in tools/ were updated for the new SystemInfo_t offsets but must be run against
a fresh Keil build.
