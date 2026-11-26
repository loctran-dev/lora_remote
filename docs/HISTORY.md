# History UI and durable push sync — 2026-11-18

## Behavior

Menu > History > choose a device > list > CENTER opens details.
LEFT/RIGHT navigate; hold RIGHT returns one level. Detail CENTER returns to the
list. Hold CENTER refreshes a frozen list while retaining its selected record
if still retained. Incoming events do not move the selection. A record whose
page has since been recycled is shown as expired rather than replaced silently.
Hidden devices remain selectable. ON/OFF callbacks are no-ops in history screens.

Each record stores slave serial, monotonically increasing event ID, timestamp,
action, source, before/after power, result, error and flags. Sources distinguish
remote, future IO1..IO4 hooks, timer, slave alarms and remote link monitoring.
ON/OFF while already in that state is recorded as No change. Power action records
also capture any active low-current alarm at that moment. Alarm raise/clear are
separate transitions; periodic alarm retransmissions are NOT new history.
Link faults/recovery are remote-local events, with Unknown motor state: losing
communication does not prove the motor stopped.

The bench RTC is currently in year 2000. Such records carry TIME_UNKNOWN and
show --:--:-- / Time not synced; no PC time or fake calendar was written.
RTC conversion was completed to include month/day/weekday, without host timezone.

## Radio protocol

CMD_HISTORY = 0x09. Request payload: HistoryPush (20-byte HistoryEvent + full
event CRC32). Response: serial, ID, full event CRC32 (12 bytes).
The full payload CRC supplements the existing legacy packet checksum; corrupt
history must be rejected BEFORE storage, otherwise a bad first copy could poison
ID deduplication.

Slave stores an event first, then pushes the oldest unsynced event. Remote
validates sender/network/paired serial/payload, queues the event, commits and
read-verifies Flash in systemTask, then queues ACK. A duplicate stored event is
ACKed again without another write. Conflicting content for an existing ID is
not ACKed. Slave verifies destination, serial, pending ID and event hash, then
programs the persistent synced halfword. It never resends synced records.

Only one event is outstanding. After EACH physical history transmission attempt,
including failure, wait >=10000 ms before another new event or retry. A startup
10-second quiet period prevents an immediate retry after reboot. No blocking
10-second sleep is used. Queue failure does not count as a physical transmission.
ACK loss causes an identical-ID resend; retry does not allocate a new event ID.

## Storage and limits

Slave standalone app: 08000000..0800DBFF (55 KiB maximum).
Slave history: 0800DC00..0800FBFF, eight 1 KiB pages, up to 248 retained events.
Slave configuration journal remains 0800FC00..0800FFFF, unchanged format.

Remote app is constrained to 08008000..08039FFF by linker/package/updater guards.
Remote history: 0803A000..0803DFFF, eight 2 KiB pages, up to 448 retained records.
Boot metadata 0803E800 and settings 0803F000..0803FFFF remain separate.
The existing remote bootloader remains installed; package validation accepts its
original advertised maximum but enforces the smaller history-safe image limit.

Records have CRC32, a commit-last word and a separate persistent sync marker.
Page headers snapshot the latest ID; remote headers also snapshot up to 20
sender high-watermarks so retention expiry does not defeat deduplication.
Torn records/headers are ignored; Flash errors never produce a successful ACK.

The slave NEVER erases a page with unsynced records. If its finite outbox fills,
control still runs, but additional logging can fail; USB DROP/FULL counters
expose that condition. These runtime counters are not a substitute for recovering
events that could not be stored. Capacity/overflow policy must be revisited for
long outages or sustained event rates over the 0.1 event/s sync budget.
Remote retention overwrites the oldest cached pages.

Normal reset and sector-only app updates preserve IDs/history. A manual mass
erase, journal erasure, or replacement slave reusing the same owner/serial is
NOT an automatically supported stream reset: current protocol has no new-stream
epoch handshake. Do not perform those operations without coordinated history/
identity migration. Never claim recovery of data physically erased.

IO processing is NOT activated, per user request. Future IO code calls
Motor_SetPower(power,HIST_IO1..HIST_IO4); physical input mapping/debounce remains
out of scope. Existing output behavior (LED1 used by current firmware) was kept;
no extra DO relay/output was enabled.

## Safety and integration

Radio TX/RX on slave are serialized by an RTOS mutex. systemTask stack grew
from 512 to 1536 bytes because history packet creation alone exceeds 512.
TX queue reduced from 16 to 8 entries to leave heap for stacks/mutexes.
Remote pauses STOP entry while received history is awaiting a durable commit/ACK.
Link error notifications do not forcibly navigate away from a history view.

Read-only USB:
- Both: HISTORY
- Remote: EVENT <physical slot> (0..447), existing DIAG/TIMER/CONFIG
- Slave: HISTORY also reports pending, last ID, sends, ACKs, DROP/FULL, heap and
  system/RX stack high-watermarks.
Slave BOOTLOADER responds ERR STLINK_ONLY; firmware updates now require ST-Link.

## Verification

ARM journal/protocol tests cover commit/readback, CRC rejection, torn writes,
deduplication and conflicting IDs, remote ACK ordering, sender validation,
persistent synced flags, FIFO retries at the 9999/10000 ms boundary, no erasure of
unsynced pages, rollover without reusing IDs, remote dedup watermark retention,
and IO hook source/state capture without configuring inputs.
Remote UI tests cover picker/list/detail/back, stable selection, empty/hidden
devices, refresh behavior and honest unsynced-time display. Calendar tests cover
leap dates and round-trip conversions.

Native evidence and final image hashes are recorded in HISTORY_NATIVE.md.

## Motor of an event (2026-11-27)

HistoryEvent.flags bits 1..3 (HIST_MODULE_MASK, 0x0E) hold the module of a
motor event (ON, OFF, ALARM, CLEAR), 0..5. Records and pushes keep their size
and CRC; older records have the bits clear and read as module 0. Link events
are device-level and keep them clear. See Lora_remote docs/KNOWN_ISSUES.md KI-12.
