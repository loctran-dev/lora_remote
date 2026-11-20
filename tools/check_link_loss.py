"""Explicit hardware link-loss test. Motor stays OFF; temporarily changes RF mode.

Requires bench settings LOST=5, slave A=2, then restores slave RUNNING in finally.
No motor ON/OFF command, Flash write, pairing edit or bootloader update is sent.
"""
import argparse
import time
import serial
from board_console import command, restart


def fields(line):
    return {key: int(value) for token in line.split()[1:] if "=" in token
            for key, value in [token.split("=", 1)]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", action="store_true", required=True)
    parser.parse_args()
    with serial.Serial("COM7", 115200, timeout=3, write_timeout=3) as motor:
        assert "PWR=0" in command(motor, "STATUS")
        try:
            print(command(motor, "BTN CONFIG LONG"), flush=True)
            assert "MODE=1" in command(motor, "MODE")
            restart()
            tries = {}
            with serial.Serial("COM6", 115200, timeout=3, write_timeout=3) as remote:
                start = time.monotonic()
                while time.monotonic() - start < 55:
                    line = command(remote, "LINK 1")
                    data = fields(line)
                    print(line, flush=True)
                    if data["PROBE"] == 1:
                        tries.setdefault(data["TRY"], time.monotonic())
                    if data["ERROR"]:
                        assert {1, 2, 3}.issubset(tries), tries
                        assert tries[2] - tries[1] >= 9
                        assert tries[3] - tries[2] >= 9
                        assert time.monotonic() - tries[3] >= 9
                        assert data["FAIL"] == 1
                        break
                    time.sleep(1)
                else:
                    raise AssertionError("No confirmed fault within the test window")
                print("PASS: three attempts, each full reply window, then error", flush=True)
                print(command(motor, "BTN CONFIRM LONG"), flush=True)
                for _ in range(8):
                    time.sleep(1)
                    line = command(remote, "LINK 1")
                    print(line, flush=True)
                    if not fields(line)["ERROR"]:
                        break
                else:
                    raise AssertionError("STATUS did not clear the connection error")
                print("PASS: fresh STATUS clears error", flush=True)
        finally:
            print(command(motor, "BTN CONFIRM LONG"), flush=True)
            print(command(motor, "MODE"), flush=True)
            assert "PWR=0" in command(motor, "STATUS")


if __name__ == "__main__":
    main()
