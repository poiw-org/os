# Doora BLE bridge (ESP32-C3 Super Mini)

A radio and nothing else: it advertises while the door is alive, takes one phone connection and carries
bytes between the phone and the door over UART. It holds no keys and no secrets and decides nothing; the
door creates every nonce and judges every request. A bridge that is replaced, reflashed or misbehaving can
refuse service, but it cannot open the door. The contract is `docs/ble-protocol.md` (sections 2, 3, 6).

It is the `bridge` environment of the PlatformIO project in `doora/`, built from `src/bridge/` plus the
shared `src/BridgeLink.cpp` (the same framing code the door compiles). The environment is selected at
build time with `ENV=bridge`; without it the make targets build the door.

## Flashing

The bridge is flashed over USB only. It has no over-the-air update and no portal; it is deliberately
simple so that it rarely changes. Run the commands from the `doora/` directory.

1. Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/). The first build
   downloads the toolchain and NimBLE-Arduino and therefore needs an internet connection.
2. Connect the Super Mini to the computer with a USB data cable. Do this while it is **not** wired to
   the door, or while the door is unpowered.
3. Build and flash:

   ```sh
   make upload ENV=bridge      # equivalent to: pio run -e bridge -t upload
   ```

   No `config.h` is needed for this environment.
4. If the serial port does not appear or the upload fails, enter download mode: hold **BOOT**, press and
   release **RESET** (or re-plug the cable while holding **BOOT**), release **BOOT**, and repeat step 3.
   With several boards connected, add `--upload-port <port>` to the `pio` command (`pio device list`
   shows the ports).
5. Verify the firmware on the bridge's console, which is its native USB serial:

   ```sh
   pio device monitor -e bridge
   ```

   The line `doora-bridge <version>` confirms that it runs. The version is `BRIDGE_FIRMWARE_VERSION` in
   `platformio.ini`; the bridge sends it to the door in `HELLO`, and the door's `status` shows it.
6. Unplug the USB cable and wire the bridge to the door as described below. In the door's console,
   `status` reports `bridge : online` within about ten seconds of both boards being powered.

To build without flashing, run `make build ENV=bridge`. The door is flashed separately; see section 3.1
of `../../README.md`.

## Wiring

| Signal | Door (ESP32) | Bridge (C3 Super Mini) |
| --- | --- | --- |
| Door TX → bridge RX | GPIO 17 (`BRIDGE_TX`) | GPIO 20 |
| Bridge TX → door RX | GPIO 16 (`BRIDGE_RX`) | GPIO 21 |
| Ground | GND | GND |
| Power | 5V / VIN | 5V |
| Optional reset | `BRIDGE_RESET` | EN/RST pad |

Check your board's pinout first: Super Mini clones differ. The USB port is the chip's native USB serial,
so GPIO 20/21 (UART0's pins, used here through UART1) are free for the link. 3.3 V logic, common ground.
Many of these boards have a weak antenna: keep it clear of metal and measure the RSSI where people will
stand, since the app's proximity threshold depends on it.

## Behaviour

- Advertises the service UUID in the advertising packet and the name `doora` in the scan response, only
  while the door pinged in the last 15 s, and only while no phone is connected.
- One connection. On subscribe it sends `CONNECTED`; the door's `CHALLENGE` and `RESULT` are notified verbatim.
  The phone's chunked writes are reassembled into one `REQUEST` (limits: in order, 1024 bytes, nothing after
  the last chunk; anything else disconnects).
- Disconnects 10 s after connect without a `RESULT`, and shortly after sending one. Sends `GONE` on every disconnect.
- Requests an ATT MTU of 247. A notification cannot be split, so a `CHALLENGE` of up to 50 bytes needs an MTU of
  at least 53; below that the bridge disconnects rather than send a truncated message (iOS always negotiates
  more; an Android app must request a larger MTU).
- A task watchdog restarts it if its loop stalls. Logs only to the USB console.
- Library: NimBLE-Arduino 2.5.2 (pinned; 2.x changed the callback signatures from 1.x).
