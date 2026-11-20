# USB update variant — STM32F103RCT6 remote

Documentation is stored in `docs/`. All paths and commands below are relative
to the project root `FIRMWARE/Lora_remote`, not this documentation directory.

This directory is an isolated snapshot of the working firmware plus a new USB
CDC bootloader. The original firmware outside this directory is unchanged.
This variant is now installed on the connected board. See DEPLOYMENT.md for
hardware verification and the pre-install flash backup.

## Files

- `Bootloader/MDK-ARM/USB_Bootloader.uvprojx`: bootloader, ARM Compiler 5.
- `Application/MDK-ARM/USB_Application.uvprojx`: relocated application.
- `dist/factory.hex`: bootloader + application + committed CRC metadata.
- `dist/application.bin`: application-only image for subsequent USB updates.
- `tools/usb_update.py`: PC updater (requires pyserial).

Always build USB_Application for this layout. Do not use the original
LoRaBasedWPC target to update a bootloader-equipped board. The snapshot .ioc and
original .uvprojx are retained as generation inputs; CubeMX regeneration does
not manage the custom linker layout. Preserve `application.sct`, the VTOR setup,
and the BOOTLOADER command when regenerating code.

## Flash layout

| Region | Start | End inclusive |
| --- | --- | --- |
| Bootloader, 32 KiB | 0x08000000 | 0x08007FFF |
| Application, 218 KiB | 0x08008000 | 0x0803E7FF |
| Commit metadata, 2 KiB | 0x0803E800 | 0x0803EFFF |
| Existing settings, 4 KiB | 0x0803F000 | 0x0803FFFF |

Bootloader accepts writes only in the application partition and its own metadata
page. It checks size, sequential offsets, each transport CRC, flash read-back,
whole-image CRC32, SRAM stack range and Thumb reset vector. A valid metadata
magic is written last; startup checks the image CRC before jumping. An interrupted
update may lose the application, but should leave the bootloader available for a
complete retry. This is single-slot recovery, not automatic rollback.

## Build and package

Open and build both named targets in Keil. Then:

```powershell
python tools/make_package.py
python tools/test_update.py
```

`prepare_projects.py` derives the two project files from the isolated snapshot
if they need to be recreated. Application source changes must be made in
`Application/`, or deliberately ported there from the original.
Run these commands from the FIRMWARE/Lora_remote directory.

## First installation — requires ST-Link once

Before replacing the flash layout, read and save a full 256 KiB flash backup,
especially settings at 0x0803F000. Program `dist/factory.hex` with ST-Link and
verify; use page/sector erase for the image, not a mass erase if settings must
be preserved. The HEX contains no settings bytes. Keep the backup for recovery.
No option bytes or write-protection bits are changed by this package.

The first installation has been completed. The physical board now boots through
the bootloader at 0x08000000 and runs USB_Application at 0x08008000.

## Later updates — USB only

```powershell
python tools/usb_update.py dist/application.bin --port COM6 --request
```

The relocated application acknowledges `BOOTLOADER` + LF, puts 0xB007 in backup
register DR10 and resets. Bootloader clears the request, detaches/re-attaches
USB using PA10/USB_CTRL, and exposes CDC. Port may differ on another PC; select
the port shown by Windows. Do not start an update while another tool owns COM.

For recovery, hold the CENTER button (PC9, active high) while powering on or
resetting, then release it. An invalid or missing application stays in the
bootloader automatically. LED1 blinks; LCD backlight is off in bootloader mode.
Upload without `--request` when already in bootloader:

```powershell
python tools/usb_update.py dist/application.bin --port COM6
```

USB_CTRL/PA10 must be high for attach. Bootloader uses the existing 8 MHz HSE
and 72 MHz system/48 MHz USB clock. It does not initialize LoRa or send motor
commands. Existing USB UI/debug commands are available only in the application.

## Protocol and validation limits

One request at a time: 16-byte little-endian header `<4sIII>` with magic UPD1,
opcode, argument and payload length; 0..256 payload bytes; then IEEE CRC32 of
header+payload as little-endian uint32. Replies are newline-terminated ASCII.
Opcodes: INFO=1, BEGIN=2 (argument=image size, payload=image CRC32), WRITE=3
(argument=offset), END=4, RUN=5. END commits only a complete image; RUN resets.

Host tests check the built HEX/BIN partitions/vectors/metadata, packet framing,
full updater transfer against a simulated responder and refusal to RUN after a
write error. They do not execute the STM32 bootloader. Hardware tests have now
verified USB entry, full application update, CRC/flash readback, reset/jump,
rejected malformed requests and preservation of settings. Actual power-loss
recovery and the physical hold-CENTER recovery gesture have not been tested.
