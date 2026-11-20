"""Explicit USB bench helpers. Import only; never runs or switches motor power."""
import time
import serial


def command(port, text):
    port.reset_input_buffer()
    port.write((text + "\n").encode())
    reply = port.readline().decode().strip()
    if not reply or reply.startswith("ERR"):
        raise RuntimeError(f"{text}: {reply!r}")
    return reply


def button(port, key, action="CLICK", screen=None):
    reply = command(port, f"BTN {key} {action}")
    if screen and f"SCREEN={screen}" not in reply:
        raise RuntimeError(f"Unexpected screen: {reply}")
    time.sleep(0.22)
    return reply


def numeric(port, old, new, editor):
    """Four-digit editor, starts at digit zero. Caller verifies current value."""
    for position, (before, after) in enumerate(zip(f"{old:04d}", f"{new:04d}")):
        increment = (int(after) - int(before)) % 10
        key, count = ("LEFT", increment) if increment <= 5 else ("RIGHT", 10 - increment)
        for _ in range(count):
            button(port, key, screen=editor)
        if position != 3:
            button(port, "CENTER", screen=editor)


def restart(port_name="COM6"):
    """Run the existing app through its bootloader, WITHOUT erasing/reflashing."""
    from usb_update import INFO, RUN, transact
    with serial.Serial(port_name, 115200, timeout=5, write_timeout=3) as port:
        assert command(port, "BOOTLOADER") == "OK BOOTLOADER"
    time.sleep(1.5)
    with serial.Serial(port_name, 115200, timeout=5, write_timeout=3) as port:
        print(transact(port, INFO).decode().strip())
        print(transact(port, RUN).decode().strip())
    deadline = time.monotonic() + 15
    while True:
        time.sleep(0.3)
        try:
            with serial.Serial(port_name, 115200, timeout=2, write_timeout=3) as port:
                print(command(port, "CONFIG"))
            return
        except serial.SerialException:
            if time.monotonic() > deadline:
                raise
