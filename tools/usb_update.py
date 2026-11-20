"""Stop-and-wait USB CDC updater for the isolated remote bootloader."""
import argparse
import struct
import time
import zlib
from pathlib import Path

APP_START = 0x08008000
META_START = 0x0803E800
MAX_IMAGE = 0x0803A000 - APP_START  # Preserve the durable history journal.
INFO, BEGIN, WRITE, END, RUN = range(1, 6)


def packet(command, argument=0, payload=b""):
    if len(payload) > 256:
        raise ValueError("Payload exceeds 256 bytes")
    body = struct.pack("<4sIII", b"UPD1", command, argument, len(payload)) + payload
    return body + struct.pack("<I", zlib.crc32(body))


def validate_image(data):
    if not 8 <= len(data) <= MAX_IMAGE:
        raise ValueError("Image does not fit the application partition")
    stack, reset = struct.unpack_from("<II", data)
    if not (0x20000000 < stack <= 0x2000C000 and stack % 8 == 0 and
            reset & 1 and APP_START <= (reset & ~1) < APP_START + len(data)):
        raise ValueError("Wrong application vectors: build USB_Application, not the original target")


def transact(port, command, argument=0, payload=b""):
    port.write(packet(command, argument, payload))
    response = port.read_until(b"\n", 128)
    expected = {INFO: b"OK INFO ", BEGIN: b"OK BEGIN\n", WRITE: b"OK WRITE ",
                END: b"OK END\n", RUN: b"OK RUN\n"}[command]
    if not response.startswith(expected):
        raise RuntimeError(f"Bootloader rejected command {command}: {response!r}")
    if command == WRITE and response != f"OK WRITE {argument + len(payload)}\n".encode():
        raise RuntimeError("Unexpected write offset acknowledgement")
    return response


def update(port, image, progress=print):
    validate_image(image)
    info = transact(port, INFO).decode().strip()
    import re
    reported = re.search(r"MAX=(\d+)", info)
    if (f"APP={APP_START:08X}" not in info or not reported or
            not MAX_IMAGE <= int(reported[1]) <= META_START - APP_START):
        raise RuntimeError("Bootloader layout differs from this updater")
    progress(info)
    transact(port, BEGIN, len(image), struct.pack("<I", zlib.crc32(image)))
    for offset in range(0, len(image), 256):
        transact(port, WRITE, offset, image[offset:offset + 256])
        if offset % 4096 == 0:
            progress(f"Written {min(offset + 256, len(image))}/{len(image)} bytes")
    transact(port, END)
    progress(f"CRC verified: {zlib.crc32(image):08X}")
    transact(port, RUN)
    progress("Reset requested; application USB will reconnect")


def main():
    import serial
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--port", default="COM6")
    parser.add_argument("--request", action="store_true", help="Ask the boot-enabled application to reset first")
    args = parser.parse_args()
    image = args.image.read_bytes()
    validate_image(image)
    if args.request:
        with serial.Serial(args.port, 115200, timeout=5, write_timeout=5) as port:
            port.reset_input_buffer()
            port.write(b"BOOTLOADER\n")
            response = port.readline()
            if response != b"OK BOOTLOADER\n":
                raise RuntimeError(f"Application has no bootloader command: {response!r}")
        time.sleep(1.5)
    deadline = time.monotonic() + 15
    while True:
        try:
            port = serial.Serial(args.port, 115200, timeout=30, write_timeout=10)
            break
        except serial.SerialException:
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.25)
    with port:
        port.reset_input_buffer()
        update(port, image)


if __name__ == "__main__":
    main()
