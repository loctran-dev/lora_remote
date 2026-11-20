# Deployment — 2026-11-08

Installed on STM32F103RC (device ID 0x414, 256 KiB flash), ST-Link serial
3E48130012144D43574D4E00. Physical application USB: COM6.

## Verified on hardware

1. Dumped full 256 KiB flash BEFORE changing its layout.
2. Installed dist/factory.hex using ST-Link; flash verification passed.
3. Bootloader validated metadata and jumped into the relocated application.
   BATTERY command and LCD screenshot both worked.
4. Ran the command below. Application acknowledged BOOTLOADER and reset;
   USB bootloader reported APP=08008000 MAX=223232 VALID=1.
5. Transferred all 103804 application bytes over USB, CRC32=91CCC3E0.
   END committed successfully; RUN reset into the application.
6. Queried BATTERY again: application communication was operational.
7. Used ST-Link in HOTPLUG read-only mode to dump the entire flash afterward.
   All factory HEX bytes matched, including boot/application/metadata.
   Configuration at offsets 0x3F000..0x3FFFF was identical to the original backup.
8. USB bootloader rejected an intentionally bad frame CRC and oversized BEGIN;
   INFO still reported a valid application, and RUN returned to application.

The USB update itself did not use ST-Link. ST-Link was used for initial install
and independent flash readback verification.

## Next updates

Work on `Application/` and build target USB_Application.
Run commands from FIRMWARE/Lora_remote:

```powershell
python tools/make_package.py
python tools/usb_update.py dist/application.bin --port COM6 --request
```

Do not program the old root LoRaBasedWPC image at 0x08000000 onto this board;
that would overwrite the bootloader. No ST-Link is needed for normal updates.

## Backup

`backups/before-install-20261108/full-flash.bin` (262144 bytes)

SHA256: d8f2ac46d6fbf9730167a16fed0239ce56135bbd5fdafecf4aff146dfa53d75b

`after-usb-update.bin` is the independent post-update flash dump.
These backups include device configuration; keep them with the project.

## Remaining verification limits

The physical CENTER-at-reset recovery gesture and an actual interrupted-power
update have not been tested. No option bytes or flash write-protection were
changed. The current architecture recovers by re-uploading the application;
there is no second image slot or automatic rollback.
