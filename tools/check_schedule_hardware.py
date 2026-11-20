"""Explicit no-load motor bench test: UI 00:01 must turn motor OFF in 60 s.

Requires user approval and --allow-on. Does not flash firmware or save settings.
Always sends remote OFF in finally. Start on normal Control, device M11, motor OFF.
"""
import argparse
import re
import subprocess
import sys
import time
from pathlib import Path
import serial
from board_console import command, button


def open_remote():
    return serial.Serial("COM6", 115200, timeout=4, write_timeout=3)


def power(motor):
    line = command(motor, "STATUS")
    print(line, flush=True)
    return int(re.search(r"PWR=(\d+)", line)[1])


def timer_snapshot():
    with open_remote() as remote:
        line = command(remote, "TIMER")
    print(line, flush=True)
    return {key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", line)}


def capture(prefix, suffix):
    if prefix:
        subprocess.run([sys.executable, str(Path(__file__).with_name("capture_screen.py")),
                        f"{prefix}-{suffix}.bmp"], check=True, timeout=15)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--allow-on", action="store_true", required=True)
    parser.add_argument("--capture-prefix", type=Path, help="Save early, middle and final LCD frames")
    args = parser.parse_args()
    passed = False
    captured_early = captured_middle = False
    confirmed = False
    before = None
    with serial.Serial("COM7", 115200, timeout=4, write_timeout=3) as motor:
        assert power(motor) == 2, "Motor must be OFF before this test"
        try:
            with open_remote() as remote:
                before = command(remote, "CONFIG")
                print(before, flush=True)
                assert "ACTIVE_NET=1" in before and "ACTIVE_FREQ=433" in before
                button(remote, "OFF", screen="CONTROL")
                for _ in range(19):
                    button(remote, "LEFT", screen="CONTROL")
                print(button(remote, "ON", "LONG", "CONTROL"), flush=True)
                assert power(motor) == 1, "Remote ON was not received"
                button(remote, "CENTER", screen="CONTROL")  # Timer picker, default Custom.
                button(remote, "CENTER", screen="CONTROL")  # Edit digit 0.
                for _ in range(3):
                    button(remote, "CENTER", screen="CONTROL")
                button(remote, "LEFT", screen="CONTROL")  # 00:01 (HH:MM).
                print(button(remote, "CENTER", "LONG", "CONTROL"), flush=True)
                start = time.monotonic()
                print("SCHEDULE 00:01 sent; monitoring motor for 65 seconds", flush=True)
            while True:
                elapsed = time.monotonic() - start
                print(f"elapsed={elapsed:.1f}s", flush=True)
                state = power(motor)
                if state == 2:
                    passed = confirmed and 55 <= elapsed <= 66
                    print("PASS auto-OFF" if passed else "FAIL switched OFF too early", flush=True)
                    break
                timer = timer_snapshot()
                assert timer["TARGET"] - timer["SYNC"] == 60, "UI did not submit exactly one minute"
                if timer["STATE"] >= 3:
                    print(f"FAIL: timer command rejected/timed out, state={timer['STATE']}", flush=True)
                    break
                confirmed = confirmed or (timer["STATE"] == 2 and timer["PWR"] == 1 and
                                          timer["DEADLINE"] == timer["TARGET"])
                if confirmed and not captured_early:
                    capture(args.capture_prefix, "early")
                    captured_early = True
                elif confirmed and elapsed >= 25 and not captured_middle:
                    capture(args.capture_prefix, "middle")
                    captured_middle = True
                if elapsed >= 65:
                    print("FAIL: motor is still ON after the one-minute timer", flush=True)
                    break
                time.sleep(min(5, 65 - elapsed))
        finally:
            try:
                with open_remote() as remote:
                    print("CLEANUP", button(remote, "OFF", "LONG", "CONTROL"), flush=True)
                    after = command(remote, "CONFIG")
                    print(after, flush=True)
                    assert before is None or before == after, "CONFIG changed during the test"
            finally:
                # Read the slave even if the remote USB cleanup itself fails.
                time.sleep(1)
                assert power(motor) == 2, "OFF cleanup not confirmed; stop bench test immediately"
            capture(args.capture_prefix, "off")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
