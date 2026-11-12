# Doora app

Flutter app (iOS 15+, Android 8+) for the po/iw door: sign in, see your virtual keys, set this
phone up for one, and unlock the door over Bluetooth with the phone and the door offline. The
protocol is `docs/ble-protocol.md`; the design is the web app's (`web/src/app.css`).

```
make run      # run on a device (the simulator has no Bluetooth)
make check    # format, analyze, test
make tokens   # regenerate colour tokens from the web CSS
```

- `lib/door/ble_protocol.dart` is the phone side of the protocol, pure Dart.
- `lib/door/door_unlocker.dart` runs the unlock over an abstract transport; tests use an
  in-process fake door (`test/support/fake_door.dart`) that checks the real ECDSA signature.
- `ios/Runner/DeviceKey.swift`, `android/.../DeviceKey.kt`: the Secure Enclave / Keystore keys.
- `ios/DoorActivity/`: the "door at hand" Live Activity. `android/.../DoorScanReceiver.kt`:
  background discovery.
