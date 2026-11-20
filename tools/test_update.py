"""Host protocol and real built-artifact checks; does not emulate STM32 execution."""
import struct
import unittest
import zlib
from pathlib import Path
from usb_update import APP_START, MAX_IMAGE, META_START, packet, validate_image, update
from make_package import read_hex, write_hex
import tempfile

ROOT = Path(__file__).resolve().parents[1]


class FakeBootPort:
    def __init__(self, fail_write=False):
        self.reply = b""
        self.image = bytearray()
        self.target_size = self.crc = 0
        self.finished = self.ran = False
        self.fail_write = fail_write

    def write(self, data):
        self.assert_packet(data)
        _, operation, argument, length = struct.unpack_from("<4sIII", data)
        payload = data[16:-4]
        if operation == 1:
            self.reply = f"OK INFO APP={APP_START:08X} MAX={MAX_IMAGE} VALID=0\n".encode()
        elif operation == 2:
            self.target_size, self.crc = argument, struct.unpack("<I", payload)[0]
            self.reply = b"OK BEGIN\n"
        elif operation == 3:
            if self.fail_write:
                self.reply = b"ERR PROGRAM\n"
                return
            assert argument == len(self.image)
            self.image.extend(payload)
            self.reply = f"OK WRITE {len(self.image)}\n".encode()
        elif operation == 4:
            assert len(self.image) == self.target_size
            assert zlib.crc32(self.image) == self.crc
            self.finished = True
            self.reply = b"OK END\n"
        elif operation == 5:
            assert self.finished
            self.ran = True
            self.reply = b"OK RUN\n"

    @staticmethod
    def assert_packet(data):
        magic, _, _, size = struct.unpack_from("<4sIII", data)
        assert magic == b"UPD1" and size <= 256 and len(data) == size + 20
        assert zlib.crc32(data[:-4]) == struct.unpack_from("<I", data, len(data) - 4)[0]

    def read_until(self, *_):
        return self.reply


class UpdateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.image = (ROOT / "dist/application.bin").read_bytes()
        cls.factory = read_hex(ROOT / "dist/factory.hex")

    def test_real_application_vectors(self):
        validate_image(self.image)

    def test_factory_vectors_and_metadata(self):
        memory = self.factory
        read = lambda address, size: bytes(memory[address + i] for i in range(size))
        length, crc, inverted, magic = struct.unpack("<IIII", read(META_START, 16))
        self.assertEqual(length, len(self.image))
        self.assertEqual(crc, zlib.crc32(self.image))
        self.assertEqual(inverted, ~length & 0xFFFFFFFF)
        self.assertEqual(magic, 0x57425043)
        self.assertEqual(read(APP_START, length), self.image)
        boot_stack, boot_reset = struct.unpack("<II", read(0x08000000, 8))
        self.assertTrue(0x20000000 < boot_stack <= 0x2000C000)
        self.assertTrue(0x08000000 <= (boot_reset & ~1) < APP_START)
        self.assertFalse(any(address >= 0x0803F000 for address in memory))

    def test_wrong_original_image_rejected(self):
        bad = bytearray(self.image)
        struct.pack_into("<I", bad, 4, 0x08000101)
        with self.assertRaises(ValueError):
            validate_image(bad)

    def test_size_and_stack_bounds(self):
        for size in (0, 7, MAX_IMAGE + 1):
            with self.assertRaises(ValueError):
                validate_image(bytes(size))
        bad = bytearray(self.image)
        struct.pack_into("<I", bad, 0, 0x2000C008)
        with self.assertRaises(ValueError):
            validate_image(bad)

    def test_entire_transfer_including_final_short_block(self):
        port = FakeBootPort()
        update(port, self.image, progress=lambda _: None)
        self.assertEqual(port.image, self.image)
        self.assertTrue(port.finished and port.ran)

    def test_flash_failure_never_runs(self):
        port = FakeBootPort(fail_write=True)
        with self.assertRaises(RuntimeError):
            update(port, self.image, progress=lambda _: None)
        self.assertFalse(port.finished or port.ran)

    def test_packet_crc_and_payload_bounds(self):
        encoded = packet(3, 0, bytes(range(256)))
        FakeBootPort.assert_packet(encoded)
        with self.assertRaises(ValueError):
            packet(3, 0, bytes(257))

    def test_factory_hex_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "roundtrip.hex"
            write_hex(self.factory, path)
            self.assertEqual(read_hex(path), self.factory)


if __name__ == "__main__":
    unittest.main(verbosity=2)
