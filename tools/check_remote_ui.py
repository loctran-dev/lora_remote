"""Explicit non-destructive remote UI stress test; never saves or controls motors."""
import argparse
import time
import serial
from board_console import command


def button(port, key, action="CLICK", screen=None):
    reply = command(port, f"BTN {key} {action}")
    if screen and f"SCREEN={screen}" not in reply:
        raise RuntimeError(f"Unexpected screen: {reply}")
    return reply


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", action="store_true", required=True)
    parser.add_argument("--rounds", type=int, default=100)
    args = parser.parse_args()
    with serial.Serial("COM6", 115200, timeout=5, write_timeout=3) as port:
        before = command(port, "CONFIG")
        print(before, flush=True)
        screen = button(port, "OFF").split("SCREEN=")[-1]
        if screen in ("EDIT_GENERAL_SETTING", "EDIT_DEVICE_SETTING"):
            screen = button(port, "RIGHT", "LONG").split("SCREEN=")[-1]
        if screen == "GENERAL_SETTING":
            button(port, "RIGHT", "LONG", "MENU")
            screen = "MENU"
        if screen == "MENU":
            for _ in range(3):
                button(port, "LEFT", screen="MENU")
            button(port, "RIGHT", screen="MENU")
            button(port, "CENTER", screen="CONTROL")
            screen = "CONTROL"
        if screen == "DEVICE_SETTING":
            button(port, "RIGHT", "LONG", "CONTROL")
            screen = "CONTROL"
        if screen != "CONTROL":
            raise RuntimeError(f"Cannot safely normalize screen {screen}")
        button(port, "RIGHT", "LONG", "DEVICE_SETTING")
        start = time.monotonic()
        for index in range(args.rounds):
            button(port, "CENTER", screen="EDIT_DEVICE_SETTING")
            button(port, "RIGHT", "LONG", "DEVICE_SETTING")
            button(port, "RIGHT", "LONG", "CONTROL")
            button(port, "RIGHT", "LONG", "DEVICE_SETTING")
            if index % 10 == 0:
                for _ in range(4):
                    button(port, "RIGHT", screen="DEVICE_SETTING")
                for _ in range(4):
                    button(port, "LEFT", screen="DEVICE_SETTING")
                print(f"round {index + 1}: {command(port, 'DIAG')}", flush=True)
        after = command(port, "CONFIG")
        assert before == after, (before, after)
        print(command(port, "LINK 1"), flush=True)
        print(command(port, "DIAG"), flush=True)
        print(f"PASS {args.rounds} rounds / {args.rounds * 4} screen transitions, "
              f"{time.monotonic() - start:.1f}s; config unchanged", flush=True)


if __name__ == "__main__":
    main()
