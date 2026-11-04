# BLE challenge authentication: architecture and plan

Goal: a member opens the door from a Flutter app (iOS and Android) **with no internet on the phone or the
door**, over Bluetooth LE. The door keeps deciding locally, exactly like an NFC tap.

## 1. Design in one paragraph

The phone holds a **hardware-backed P-256 key pair** (Secure Enclave / Android Keystore) that never leaves
the device, plus a **credential**: the same RS256 JWT the backoffice already signs for keys, with one new
claim `cnf` = the phone's public key. The door sends a random nonce; the phone returns the credential and a
signature over the nonce made with its private key. The door checks (1) the credential with the public key
it already has, (2) the existing `decide()` rules and blacklist, (3) the signature with the key inside the
credential. All three work offline. A credential on its own is useless to an eavesdropper (no private key),
and a signature on its own is useless later (nonce is single-use).

```
Phone (Flutter)                                   Door (ESP32 WROOM-32, NimBLE)
 Secure Enclave / Keystore: device key             RSA public key (flash), blacklist, clock
 credential JWT (RS256 + cnf)
        │  scan for service UUID, connect, MTU 247          │ advertises Doora service
        │◄──────────── CHALLENGE: ver, nonce(16), doorId ───┤ esp_random, valid 5 s, single use
        │  user gesture + biometric → sign(…)               │
        ├────────── PRESENT: credential, sig(64) ──────────►│ queue to loop(); BLE callback does no crypto
        │                                                   │ jwtVerify → decide() → blacklist → ECDSA verify
        │◄──────────── RESULT: granted | reason, delay_ms ──┤ actuators.grant(), journal record (flag BLE)
```

Signed message = `SHA256("doora-ble-v1" ‖ len‖doorId ‖ nonce ‖ SHA256(credential))`. Binding the door id stops
a signature captured at one door from working at another; binding the credential hash stops swapping
credentials under a valid signature.

## 2. Why this shape (decisions)

| Decision | Choice | Reason |
| --- | --- | --- |
| Credential format | Existing key JWT + `cnf` claim | Door already verifies RS256 offline, has `decide()`, blacklist, expiry, journal. No new trust root. |
| A phone is bound to a virtual key | `keys.device_pubkey`, `device_name`, `device_bound_at` on keys with `serial_number is null`; one phone per virtual key, binding is permanent (migration `20261104030000`) | Disable, renew, expiry, blacklist, audit log and the member "my keys" UI all work unchanged. An admin controls how many phones a user may have by how many virtual keys they issue. Moving to a new phone = new virtual key, old one disabled. |
| Who may use the app | Only users holding a usable virtual key (not disabled, in window, not blocked): `public.can_use_app()` | The app calls it after sign-in and on every refresh and wipes its credential and refuses to unlock if it is false. The edge function enforces the same rule server-side. A user with only a tag key gets no app. |
| Phone key algorithm | ECDSA P-256 | The only curve both Secure Enclave and Android Keystore do in hardware. Ed25519 and RSA are not hardware-backed on iOS. |
| Signature on the wire | Raw 64-byte `r‖s` | Door code stays trivial. Phone converts from the DER both OS APIs return (about 15 lines). |
| Door crypto | micro-ecc (vendored, `uECC_secp256r1`) behind `Ecdsa.h` | Portable and allocation-free like `Rsa.cpp`, so host tests run with no mbedTLS. Fallback: mbedTLS (already linked for TLS, zero flash) behind the same header if verify is too slow (see risk 3). |
| BLE stack | NimBLE-Arduino, peripheral only, 1 connection | Roughly half the flash and a fraction of the RAM of Bluedroid. RAM, not flash, is the real constraint (section 6). |
| Pairing / bonding | None. Application-layer auth only | Nothing secret crosses the link, so no bonding table and no pairing delay. |
| Door-side session | One at a time, 8 s hard timeout, one attempt per connection | Bounds CPU and abuse. |
| Credential lifetime | Short, default 7 days, refreshed by the app while online | Bounds how long a lost phone stays valid on a door that missed the blacklist update. NFC keys last far longer. |
| Foreground unlock only (v1) | Tap button in app | Background BLE on iOS is restrictive and background auto-unlock widens the relay window. Revisit later. |

## 3. Threat model (be honest in the docs)

| Threat | Mitigation | Residual |
| --- | --- | --- |
| Passive sniff, replay later | Fresh 16-byte nonce per connection, single use, 5 s TTL, signature covers it | None |
| Stolen credential (sniffed or backed up) | Needs the non-exportable private key | None |
| Phone lost or stolen | Key needs biometric/passcode per signature; member or admin disables the key; credential expires in at most 7 days | Door offline and not yet synced: works until the credential expires or the door syncs. Same limit as NFC today, tighter bound. |
| Relay attack (attacker bridges phone and door in real time) | Door id in signed message, 5 s window, user gesture + biometric on the phone, optional RSSI floor on the door | **Cannot be fully stopped** by BLE alone. Same class as NFC relay. Accept for a hackerspace, document it. |
| Rogue "door" harvests credentials | Credential is useless without the key. Phone only offers it to devices advertising the service UUID | Leaks a user id (uuid). Optional later: LE Secure Connections encryption. |
| CPU exhaustion via junk PRESENT | Check order: RSA credential first (cheap), `decide()` second, ECDSA only if both pass; 1 session at a time; 1 s penalty before issuing the next challenge after a failure | A BLE jammer can always block BLE. NFC is unaffected. |
| Clock wrong after blackout | Same as NFC: estimate only lags, so an expired credential may pass until sync | Bounded by the short TTL |
| Tampered door-side clock via forged SNTP | Existing handling (`clock_.reliable()`) | Unchanged |

Deny reasons are returned only after the credential signature verified, so an anonymous client learns nothing.

## 4. Backend (Supabase)

1. **Migration `20261104030000_virtual_key_device.sql`** (written, not yet applied)
   - Adds `device_pubkey` (87-char base64url of the 65-byte P-256 point), `device_name`, `device_bound_at`
     to `keys`, with checks: format, only on virtual keys, pubkey and bound-at set together, name length.
   - Unique index on `device_pubkey`. Trigger `guard_device_binding`: once set, it can never change or clear.
   - `public.can_use_app()` (caller) and `public.can_use_app_as(uuid)` (service role): the app gate above.
   - Members and admins have no column grant on the new columns, so only the edge function (service role)
     can bind a device. `entries` dedupe is already `nulls not distinct`, so BLE opens with no serial upload
     safely. Existing virtual keys are untouched: they just have no phone yet.
   - No cascade trigger needed: the phone *is* the virtual key, so disabling it blacklists the phone on the
     door through `door_sync()`.
2. **Extract signing** from `functions/keys/index.ts` into `functions/_shared/credentials.ts`
   (`signKey`, `base64Url`). Add optional `cnf` (device public key, base64url of the 65-byte point) and
   `aud: "ble"`. `keys` imports it; behaviour unchanged for tags.
3. **New edge function `ble-credential`** (member auth via `callerId`, like `unlock`):
   - `enroll {key_id, public_key, name}`: caller must own that virtual key and pass `can_use_app_as`; the key
     must have no device yet. Validate the point is on P-256 (WebCrypto `importKey("raw", …, ECDSA P-256)`
     rejects bad points), set `device_pubkey/name/bound_at` (conditional on `device_pubkey is null`, so a
     race binds once), return `{credential, expires_at}`. If the user has several unbound virtual keys the
     app lets them pick; with exactly one it is automatic.
   - `refresh {key_id}`: owner only, key bound, not disabled, in window, user not blocked. Re-signs a
     credential with `exp = min(now + TTL, key.expires_at)` and `nbf = key.not_before`. **The key row is not
     modified**, so the admin's validity window and the blacklist pruning are unaffected.
   - Refusal: no usable virtual key returns 403 `no_virtual_key`; the app treats it as "not entitled".
   - Env `BLE_CREDENTIAL_TTL_DAYS` (default 7).
   - The admin `renew` action in `keys/index.ts` must keep `cnf` and `aud: "ble"` in the re-signed token
     for bound keys (or leave `keys.jwt` alone for them), otherwise the stored `jwt` loses the binding.
   - Revoke a phone: member disables their own key row (existing RLS). Admin: existing Keys page.
4. **Backoffice UI** (small): show device keys with `device_name` and a Bluetooth badge in `keys/+page.svelte`
   and `ActivityRow.svelte`.

## 5. Firmware (`doora/`)

New files, following the existing split of portable core vs. hardware glue:

| File | Role | Host-testable |
| --- | --- | --- |
| `Ecdsa.h/.cpp` | `bool ecdsaP256Verify(pub[64], hash[32], sig[64])` (micro-ecc, vendored under `src/third_party/`) | yes |
| `BleProto.h/.cpp` | Framing (type, length, chunk reassembly into a ~1 KB buffer), challenge/response message build and parse, signed-message hash | yes |
| `Access.cpp` addition | `decideBle(...)`: verified `KeyClaims` + nonce state + signature check result → `Verdict` | yes |
| `Ble.h/.cpp` | NimBLE service: one GATT service, `rx` (write) and `tx` (notify) characteristics, advertising, session timer, nonce state. Callbacks only copy bytes and set an atomic flag. | no (hardware) |

Changes to existing code:

- `Jwt.h/.cpp`: add `uint8_t cnf[65]; bool hasCnf` to `KeyClaims`; parse `cnf`; accept `aud == "ble"` as a
  separate audience kind. Today `parseUid` rejects non-hex `aud`, which is what keeps virtual keys off NFC
  tags. Keep that: a token with `cnf`/`aud:"ble"` must never be accepted from a tag, and an NFC token must
  never be accepted over BLE (check the audience kind on both paths, add tests).
- `Access.cpp`: split `decide()` so the serial check is separate from validity/blacklist, or pass an
  audience-match flag. NFC and BLE both call the shared part, so rules cannot drift.
- `main.cpp`: `handleBle(const BleRequest&)` next to `handleTag`. Only `loop()` actuates the relay and writes
  the journal, same as taps. `ble.take()/release()` mirrors `reader.take()/release()`.
- `Journal.h`: add `kBle = 16` flag; `journalToJson` emits `"source":"ble"`. Record layout unchanged (flag
  byte has room, `uid` empty for BLE).
- `Console`: `status` prints BLE state (advertising, sessions, last result, heap low-water mark).
- `platformio.ini`: add NimBLE-Arduino (pin a version that builds against `espressif32@^6.9.0`), build flags
  to shrink it (`CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1`, central/observer roles off, log level off), and
  `board_build.partitions = partitions.csv`.
- **Partition table** (`doora/partitions.csv`): the current image is **1,007,584 bytes against the default
  1.31 MB app slot**, so NimBLE (+~150-250 KB, measure) leaves almost no room. Keep `nvs` at 0x9000/0x5000 so
  stored settings survive. 4 MB module: one factory app of about 2 MB plus a 256 KB LittleFS. 8/16 MB module:
  two OTA slots (enables OTA later). Moving the LittleFS partition drops the journal and blacklist, so let the
  door upload its journal before the first flash with the new table; the blacklist refetches by itself.
  Confirm the module's real flash size first (WROOM-32 is 4 MB; WROOM-32E comes in 4/8/16 MB).

Unlock latency budget: connect + MTU about 300-600 ms, credential transfer 4 writes about 100 ms, RSA verify
tens of ms, ECDSA verify 100-400 ms (to measure), relay fires immediately after. Target under 1.5 s from tap.

## 6. Risks to retire first (ordered)

1. **Heap.** The door already runs two concurrent TLS sessions (about 80 KB) and restarts when free heap is
   under 7 KB. Measure `status` before and after NimBLE init and during an upload. Gate: lowest free heap
   during upload stays at least 20 KB above today's low-water mark. If not: BLE session refuses while an
   upload TLS is open (the websocket keeps priority), or trim NimBLE buffers further.
2. **Wi-Fi + BLE coexistence.** One radio. Watch the websocket heartbeat (10 s ping, 4 s timeout) while
   advertising at 160 ms and while a session runs. Tune the advertising interval before anything else.
3. **ECDSA speed** with micro-ecc on Xtensa. Gate: verify under 400 ms, else swap to the mbedTLS backend.
4. **Hardware key access from Flutter.** No first-party Flutter API signs with a non-exportable key. Options:
   a small platform channel (about 100 lines Swift + Kotlin: `generate`, `sign`, `delete`) or an existing
   package; spike both and pick. Requirements: Secure Enclave on iOS, Keystore (StrongBox if present) on
   Android, per-use user authentication, key invalidated on biometric enrollment change.
5. **Host-test parity.** Cross-check micro-ecc against signatures produced by a real phone (golden vectors
   generated by the Flutter spike) so Dart, Swift/Kotlin and C++ agree on byte layout.

## 7. Flutter app (`app/`, new sibling of `doora/` and `web/`)

- Packages: `supabase_flutter` (login, call `ble-credential`), `flutter_blue_plus` (BLE central),
  `local_auth` (biometric prompt), `flutter_secure_storage` (credential cache), platform channel from risk 4.
- Layers: `DeviceKey` (channel wrapper), `CredentialStore` (+ refresh policy), `DoorLink` (scan, connect,
  chunk write, notify, timeouts), `UnlockController` (state machine: idle, scanning, connecting, presenting,
  granted/denied(reason)), UI.
- Enrollment: sign in, create device key, `enroll`, store credential. Show expiry and device name.
- Refresh: on app open and when online, if less than half the TTL remains, call `refresh`. Show a clear
  "connect to the internet before <date>" warning when under 24 h remain. Unlock works fully offline until expiry.
- Unlock: one big button; biometric prompt; scan filtered by the Doora service UUID; strongest RSSI wins if
  several doors; abort after 8 s.
- Permissions: Android 12+ `BLUETOOTH_SCAN`/`BLUETOOTH_CONNECT` (no location needed with `neverForLocation`),
  Android 11 and below location; iOS `NSBluetoothAlwaysUsageDescription`.
- Out of scope v1: background or automatic unlock, multiple doors UI, wearables.

## 8. Milestones

| # | Deliverable | Exit gate |
| --- | --- | --- |
| M0 | Partition table + NimBLE builds, advertising, `status` shows heap | Heap and websocket gates (risks 1-2) pass with BLE idle |
| M1 | `Ecdsa` + `BleProto` + `decideBle` portable core with host tests incl. golden vectors | `make test` green; verify time measured on board |
| M2 | Door GATT session end to end with a scripted client (nRF Connect or a small Python `bleak` script) using a test credential | Door opens; denies expired, blacklisted, wrong door id, replayed nonce; journal shows `source: ble` |
| M3 | Migration + `_shared/credentials.ts` + `ble-credential` function | Enroll and refresh work; disabling the key blacklists it on the door via `door_sync()` |
| M4 | Flutter spike: device key channel, enrollment, unlock on one iOS and one Android phone | Unlock in airplane mode (Bluetooth on) under 1.5 s |
| M5 | App polish, backoffice badges, README/docs update, security review | Relay and replay tests documented; `ecc:security-reviewer` pass |

## 9. Open decisions (recommendation in bold)

- **D1 TTL:** **7 days.** Shorter is safer, longer tolerates phones that rarely go online.
- **D2 Entitlement (decided):** only users with a usable virtual key can use the app (section 2 and 4).
- **D3 Devices per user (decided):** one phone per virtual key; the admin issues as many virtual keys as
  phones the user may have.
- **D4 Door id on the wire:** reuse `settings.doorId`. One door today; multi-door works without a protocol change.
- **D5 RSSI floor:** **off in v1** (unreliable across phones); log RSSI in the journal metadata to calibrate later.
