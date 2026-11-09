#!/usr/bin/env python3
"""Sends signed firmware to the door over Bluetooth.

Pair with the door first. Opening the port makes the connection, which an admin then approves in
the webapp unless this device is still authorized.

    ~/.platformio/penv/bin/python tools/bt_update.py /dev/cu.doora-main [firmware.bin]
"""
import sys
import time

import serial

ACK = b"\x06"
CHUNK = 1024  # src/Ota.cpp
MAGIC = b"DOORASIG"
APPROVAL_S = 25
ACK_S = 15
VERIFY_S = 30


def wait(port, seconds, wanted):
    """Reads until `wanted` arrives; log lines in between are ignored."""
    seen = b""
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        seen += port.read(port.in_waiting or 1)
        if b"UPDATE FAILED" in seen:
            seen += port.read_until(b"\n")
            sys.exit(seen[seen.index(b"UPDATE FAILED"):].decode(errors="replace").strip())
        if wanted in seen:
            return
    sys.exit("Timed out waiting for the door (%r)." % wanted)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    path = sys.argv[2] if len(sys.argv) > 2 else ".pio/build/esp32dev/firmware.bin"
    with open(path, "rb") as f:
        image = f.read()
    if not image.endswith(MAGIC):
        sys.exit("%s is not signed; the door would discard it. Run tools/ota_keygen.py and rebuild." % path)

    with serial.Serial(sys.argv[1], 115200, timeout=0.2) as port:
        print("Connected. Waiting for approval (or an existing authorization)...")
        wait(port, APPROVAL_S, b"Bluetooth: link open")
        port.write(b"update %d\n" % len(image))
        wait(port, ACK_S, ACK)
        started = time.monotonic()
        for at in range(0, len(image), CHUNK):
            port.write(image[at:at + CHUNK])
            wait(port, ACK_S, ACK)
            print("\r%3d%%" % (100 * min(at + CHUNK, len(image)) // len(image)), end="", flush=True)
        print("\nSent %d bytes in %d s. Checking the signature..." % (len(image), time.monotonic() - started))
        wait(port, VERIFY_S, b"UPDATE OK")
    print("Update installed; the door is restarting.")


if __name__ == "__main__":
    main()
