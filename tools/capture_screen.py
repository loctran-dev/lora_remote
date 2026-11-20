"""Read the remote LCD over USB and save a 24-bit BMP (no Pillow required)."""
import argparse
import struct
import time
from pathlib import Path

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--port", default="COM6")
    args = parser.parse_args()
    deadline = time.monotonic() + 15
    while True:
        try:
            connection = serial.Serial(args.port, 115200, timeout=15, write_timeout=3)
            break
        except serial.SerialException:
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.25)
    with connection as port:
        port.reset_input_buffer()
        port.write(b"SCREEN\n")
        header = port.read(12)
        if len(header) != 12 or header[:4] != b"LCF1":
            raise RuntimeError(f"Invalid LCD header: {header!r}")
        width, height, size = struct.unpack("<HHI", header[4:])
        if not width or not height or size != width * height * 2 or size > 1024 * 1024:
            raise RuntimeError("Invalid LCD dimensions")
        pixels = port.read(size)
        if len(pixels) != size:
            raise RuntimeError("Incomplete LCD frame")
    values = [v for (v,) in struct.iter_unpack("<H", pixels)]
    stride = (width * 3 + 3) & ~3
    rows = []
    for y in range(height - 1, -1, -1):
        row = b"".join(bytes(((v & 31) * 255 // 31,
                              ((v >> 5) & 63) * 255 // 63,
                              (v >> 11) * 255 // 31))
                       for v in values[y * width:(y + 1) * width])
        rows.append(row + bytes(stride - width * 3))
    data = b"".join(rows)
    bitmap = struct.pack("<2sIHHI", b"BM", 54 + len(data), 0, 0, 54)
    bitmap += struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0,
                          len(data), 0, 0, 0, 0)
    args.output.write_bytes(bitmap + data)
    print(f"Saved {width}x{height}: {args.output}")


if __name__ == "__main__":
    main()
