"""Host test of the A/B configuration storage (config_store.c) with a mocked flash.

Builds config_store.c, flash.c and utility.c with gcc against the shims in
config_store_host/ and runs the scenarios in test_config.c: blank flash,
alternating saves, a power loss before every flash operation of a save,
migration from the old single-copy format, corrupt slots, sequence wrap and
sanitising. Linux only: the fake flash is mapped at the real addresses
(0x0803F000) with mmap.
"""
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "Application/Library"
SHIM = Path(__file__).resolve().parent / "config_store_host"


@unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("gcc"), "needs Linux and gcc")
class ConfigStoreHostTest(unittest.TestCase):
    def test_scenarios(self):
        with tempfile.TemporaryDirectory() as tmp:
            build = Path(tmp)
            # Shims and sources side by side, so quoted includes find the shims first.
            for name in ["config_store.c", "config_store.h", "flash.c", "flash.h", "utility.c", "utility.h"]:
                shutil.copy(LIB / name, build)
            shutil.copy(ROOT / "Application/Core/Inc/define.h", build)
            for path in SHIM.iterdir():
                shutil.copy(path, build)
            subprocess.run(["gcc", "-std=c99", "-O1", "-w", "-fno-strict-aliasing", "-I.", "-o", "test_config",
                            "test_config.c", "config_store.c", "flash.c", "utility.c"], cwd=build, check=True)
            result = subprocess.run(["./test_config"], cwd=build, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("ALL PASSED", result.stdout)


if __name__ == "__main__":
    unittest.main()
