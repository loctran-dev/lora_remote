"""Produce application.bin and factory.hex; never connect to the board."""
import struct
import zlib
from pathlib import Path
from usb_update import APP_START, META_START, MAX_IMAGE, validate_image

ROOT = Path(__file__).resolve().parents[1]


def read_hex(path):
    memory, upper = {}, 0
    for line in path.read_text().splitlines():
        record = bytes.fromhex(line.strip().removeprefix(":"))
        if not record or sum(record) & 255 or len(record) != record[0] + 5:
            raise ValueError(f"Invalid HEX record in {path}")
        count, address, kind = record[0], int.from_bytes(record[1:3], "big"), record[3]
        if kind == 0:
            for index, byte in enumerate(record[4:4 + count]):
                location = upper + address + index
                if location in memory:
                    raise ValueError("Overlapping HEX records")
                memory[location] = byte
        elif kind == 4:
            upper = int.from_bytes(record[4:6], "big") << 16
        elif kind == 2:
            upper = int.from_bytes(record[4:6], "big") << 4
        elif kind == 1:
            break
        elif kind not in (3, 5):
            raise ValueError("Unsupported HEX record")
    if not memory:
        raise ValueError("Empty HEX image")
    return memory


def hex_record(kind, address, payload):
    body = bytes([len(payload)]) + address.to_bytes(2, "big") + bytes([kind]) + payload
    return ":" + (body + bytes([-sum(body) & 255])).hex().upper()


def write_hex(memory, path):
    lines, previous_upper = [], None
    addresses = sorted(memory)
    index = 0
    while index < len(addresses):
        address = addresses[index]
        upper = address >> 16
        if upper != previous_upper:
            lines.append(hex_record(4, 0, upper.to_bytes(2, "big")))
            previous_upper = upper
        block = bytearray([memory[address]])
        index += 1
        while (index < len(addresses) and len(block) < 16 and
               addresses[index] == address + len(block) and addresses[index] >> 16 == upper):
            block.append(memory[addresses[index]])
            index += 1
        lines.append(hex_record(0, address & 65535, block))
    lines.append(hex_record(1, 0, b""))
    path.write_text("\n".join(lines) + "\n")


def main():
    boot = read_hex(ROOT / "Bootloader/MDK-ARM/USB_Bootloader/USB_Bootloader.hex")
    app = read_hex(ROOT / "Application/MDK-ARM/USB_Application/USB_Application.hex")
    if min(boot) != 0x08000000 or max(boot) >= APP_START:
        raise ValueError("Bootloader crosses partition")
    if min(app) != APP_START or max(app) >= APP_START + MAX_IMAGE:
        raise ValueError("Application crosses partition")
    image = bytes(app.get(address, 255) for address in range(APP_START, max(app) + 1))
    validate_image(image)
    metadata = struct.pack("<IIII", len(image), zlib.crc32(image), ~len(image) & 0xFFFFFFFF, 0x57425043)
    memory = dict(boot)
    memory.update(app)
    memory.update({META_START + i: value for i, value in enumerate(metadata)})
    output = ROOT / "dist"
    output.mkdir(exist_ok=True)
    (output / "application.bin").write_bytes(image)
    write_hex(memory, output / "factory.hex")
    print(f"Bootloader: {max(boot) - min(boot) + 1} bytes")
    print(f"Application: {len(image)} bytes, CRC32={zlib.crc32(image):08X}")
    print("Created dist/application.bin and dist/factory.hex; config addresses are absent")


if __name__ == "__main__":
    main()
