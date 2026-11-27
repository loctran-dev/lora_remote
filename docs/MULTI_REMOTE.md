# Several remotes: one main remote, secondary remotes

Design decided 2026-11-27. Not implemented yet; tracked as KI-24 in
KNOWN_ISSUES.md. Line numbers refer to lora_remote c5c61bc and lora_device
origin/main fbf5aae.

## Goal

Several remotes control and receive alarms from the same devices. One main
remote administers the network; secondary remotes do the everyday tasks.

| Task | Main | Secondary |
|---|---|---|
| View status and currents | yes | yes |
| Switch a motor ON/OFF | yes | yes (the slave enforces Lock) |
| Set or cancel a timer | yes | yes |
| Receive and acknowledge alarms (KI-17) | yes | yes, each remote on its own |
| Pair or delete devices, pair secondary remotes | yes | no |
| Alarm threshold, delay, auto status, Lock, names | yes | no |
| Radio settings (net, frequency, SF) | yes | no |
| Set the device clock | yes | no |
| History acknowledgement (ACK to the slave) | yes | no |

## How it works today (why a second remote does not work)

- The frame header has the device's netId/nodeId only, no sender
  (communication_lora.h:61-72). Every remote on the same net, frequency and SF
  hears every frame.
- CMD_CONFIG_CONNECTION moves the device to the pairing remote's network, so
  pairing with a second remote takes the device away from the first.
- Transaction ids start at 0 on every boot (communication_lora.c:11), so two
  remotes produce the same ids and can take each other's replies.
- Lock is checked on the remote only (main.c Main_SetMotorPower).
- Every probe carries syncTime, so two remotes with different clocks make the
  device clock jump.
- Changing net, frequency or SF on the remote only changes the remote; the
  paired devices stay on the old settings without warning (screen_egss.c:304-324).

## Design

### 1. Secondary remote paired like a device

- A remote in "join" mode answers CMD_GET_FACTORY_INFO on the default network
  like a slave, with a new device type 'R', its STM32 UID as serial and
  moduleCount 0, and accepts CMD_CONFIG_CONNECTION. It takes the net, node,
  frequency and SF from it, stores role = secondary and saves.
- The main remote stores it in a separate table appended to the CFG3 layout,
  e.g. `remotes[4]` (serial, nodeId, deleted), not in `storageData.devices`:
  a device slot would also allocate a motor slot (Config_ModuleCount(0) = 1),
  show it on the control screen and device manager, and make the link
  monitor probe it.
- findFreeNodeId() skips the node ids of devices and remotes.
- The node id of a remote is its remote id; the main remote uses a fixed id.

### 2. Configuration over LoRa (secondary pulls)

- The main remote keeps a configuration version, incremented on every change
  a secondary needs: device added/deleted, names (KI-22), Lock.
- A secondary asks for the version after boot, on every wake-up scan and
  after a reply it did not expect. If its copy is older it requests the
  configuration in numbered parts; the whole set has a CRC and is applied
  only when complete. The main remote wakes from STOP on the request.
- Sent: device list (serial, nodeId, factoryInfo), device and motor names,
  Lock per motor (for display). Not sent: thresholds, delays, the main
  remote's own settings. Hide stays a local choice on each remote.
- The secondary writes flash only when the version changed.
- If the main remote is out of range the secondary keeps its copy and shows
  "configuration not up to date" after a while.
- A secondary deleted by the main remote gets "not a member" on its next
  request and returns to unpaired.

### 3. Transaction ids

The high byte of every transaction id is the remote id (its node id), the
low byte counts. Replies to another remote never match.

### 4. Lock enforced by the slave

- Lock is a new last field of CMD_SET_CONFIG, sent with the threshold, delay
  and auto status (decided 2026-11-27, no separate command). The slave
  already accepts two payload sizes (5 and 7 bytes, commandHandlers.c:202-203);
  it must also accept the new 8-byte payload and keep Lock unchanged for the
  shorter ones. An old slave drops an 8-byte CMD_SET_CONFIG without a reply,
  so update the slaves before the remotes (as KI-11).
- The whole CMD_SET_CONFIG (Lock, threshold, delay, auto status) is accepted
  while the motor is ON (decided 2026-11-27). Today the slave refuses it while
  ON (commandHandlers.c:212, since the initial import, reason not recorded);
  that check is removed. Check when doing it: the flash write of the settings
  stalls the CPU while the motor runs, and a new threshold or delay should
  restart the low-current debounce so the alarm is not raised or cleared by
  the change itself.
- CMD_CONTROL_POWER on a locked motor is answered with a new result code
  "locked", so every remote shows "Locked" rather than a generic failure.
- Lock does not block timers (CMD_SET_SCHEDULE), a timer set before the lock
  still switches the motor off, and the device's local buttons are not
  blocked (decided 2026-11-27).
- The status records carry the Lock state so every remote shows it without
  waiting for a configuration update. The remote keeps its own check for an
  immediate message; the slave is the final gate.

### 5. Time

- Only the main remote sets the device clock (syncTime in probes,
  CMD_SYNC_TIME). Secondaries send syncTime = 0.
- A secondary sets its own clock from `localTime` in the device status, so
  the absolute switch-off time of its timers matches the device.

### 6. Radio changes

Allowed on the main remote; afterwards every device and every secondary
remote must be paired again (decided 2026-11-27). The radio settings screen
should say so before saving.

### 7. Link monitor and airtime

- Secondaries mostly listen: replies to the main remote's probes refresh them
  too (statusGeneration). They probe with a longer timeout and a random delay.
- The LoRa driver has no channel-activity check before sending; collisions are
  left to the retries.

### 8. History

Only the main remote acknowledges history pushes, so the slave keeps one
complete copy there. A secondary keeps no history, or only the pushes it
happens to hear.

### 9. Backup and replacing the main remote

USB export/import of the main remote's configuration, and a procedure to make
another remote the main one (import the backup, set role = main).

## Known limits

- No authentication or encryption: any radio on the same net and frequency,
  including a lost remote, can still send commands.
- A slave and a joining secondary in pairing mode at the same time both answer
  and collide (as two slaves do today).

## Work list

| Part | Remote | Slave | Size |
|---|---|---|---|
| Role main/secondary, menu gating, remote table, join as type 'R' | yes | - | medium |
| Configuration version and pull in parts | yes | - | large |
| Transaction id with remote id | yes | - | small |
| Lock in CMD_SET_CONFIG, result "locked", Lock in status | yes | yes | medium |
| Clock only from the main remote, secondary takes device time | yes | - | small |
| Radio change warning | yes | - | small |
| Lighter link monitor on secondaries, history ACK only on main | yes | - | small |
| USB backup and main-remote replacement | yes | - | medium |
