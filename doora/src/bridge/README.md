# Doora BLE bridge (ESP32-C3 Super Mini)

A radio and nothing else: it advertises while the door is alive, takes one phone connection and carries
bytes between the phone and the door over UART. It holds no keys and no secrets and decides nothing; the
door creates every nonce and judges every request. A bridge that is replaced, reflashed or misbehaving can
refuse service, but it cannot open the door. The contract is `docs/ble-protocol.md` (sections 2, 3, 6).

It is the `bridge` environment of this PlatformIO project, built from `src/bridge/` plus the shared
`src/BridgeLink.cpp` (the same framing code the door compiles):

```sh
make build  ENV=bridge   # compile
make upload ENV=bridge   # flash over USB (hold BOOT while plugging in if the port does not appear)
pio device monitor -e bridge   # the console is the native USB serial
```

There is no OTA and no portal for the bridge; it is deliberately dumb so it rarely changes. Its version
(`BRIDGE_FIRMWARE_VERSION` in `platformio.ini`) is sent to the door in `HELLO` and shown by the door's `status`.

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
