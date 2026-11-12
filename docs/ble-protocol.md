# BLE unlock protocol

Normative. The door firmware, the bridge firmware and the phone app implement this file; when code
and this file disagree, one of them is a bug. Change it only together with all three.

## Parts and trust

```
phone ──BLE──► bridge (ESP32-C3 Super Mini) ──UART──► door (ESP32, the authority) ──► relay
```

- **The door decides.** It creates every nonce, verifies every credential and proof, applies the
  blacklist and dates, drives the relay and journals the attempt.
- **The bridge is a radio.** It advertises, accepts one connection, reassembles writes and forwards
  bytes. It holds no keys, no secrets and no state that matters, and it never decides anything. A
  bridge that is replaced, reflashed or misbehaving can refuse service or relay a nearby phone's
  radio link, but it cannot open the door.
- **The phone proves possession** of a P-256 private key that lives in its Secure Enclave or
  Android Keystore, and presents a credential signed by the backend that names that key.

All multi-byte integers are big-endian. "base64url" is RFC 4648 §5 without padding.

## 1. Credential

The virtual key's JWT (RS256, signed by the backend with the same key as tag tokens) with:

- `aud`: the literal string `"ble"`.
- `cnf`: base64url of the phone's 65-byte uncompressed P-256 public key (`04 || X || Y`), exactly
  87 characters. A flat string on purpose: the door's claim scanner reads flat values only.
- `sub` (user id), `jti` (key id), `iat`, `nbf`, `exp` as for every key.

The whole token is at most 860 characters (the door's `kMaxTokenLen`).

## 2. GATT (phone ⇄ bridge)

- Service `7d0a0001-6f2b-4c1e-9a53-d00a00000001`, in the advertising packet itself (iOS background
  scans match only UUIDs there). Name `doora` in the scan response. Connectable, undirected.
- Characteristic `7d0a0002-6f2b-4c1e-9a53-d00a00000001`: Write (with response) and Notify.
- No pairing, no bonding, no link-layer encryption. Security is in the messages.
- One connection at a time; others are refused while one is open.
- The bridge advertises only while it hears the door (a `PING` within the last 15 s), so a phone
  never prompts for a door that cannot answer.
- The bridge disconnects 10 s after connect if no `RESULT` has arrived, and right after sending one.

## 3. Messages (phone ⇄ door, carried verbatim by the bridge)

**Challenge**, door to phone, sent as a notification once the phone has subscribed:

```
0x01 | version 0x01 | nonce[16] | door_id_len (1) | door_id (ASCII, 1..31 bytes)
```

**Request**, phone to door. The phone writes chunks with response. Each write is
`header (1) | data`: bit 7 of the header marks the last chunk, bits 0-6 are the chunk index from 0.
The bridge reassembles them and rejects (disconnects on) an out-of-order index, more than 1024
reassembled bytes, or any write after the last chunk. The reassembled body:

```
0x02 | version 0x01 | signature[64] | jwt (ASCII, the rest of the body)
```

`signature` is ECDSA P-256 over SHA-256 of

```
"doora-ble-1" (11 ASCII bytes) | door_id_len (1) | door_id | nonce[16]
```

encoded as raw `r || s`, each 32 bytes, left-padded. The phone converts from the DER its keystore
returns; the door accepts exactly 64 bytes and parses no DER.

**Result**, door to phone, notification: `0x03 | code (1)`.

| Code | Meaning | App's sentence |
| --- | --- | --- |
| 0 | Opened | "Door open" |
| 1 | Malformed request | "Something went wrong. Try again." |
| 2 | Bad credential (signature, claims, `aud`, `cnf`) | "This key isn't valid on this door." |
| 3 | Proof failed | "This phone couldn't prove it holds the key." |
| 4 | Key disabled | "This key has been disabled." |
| 5 | Expired | "This key has expired." |
| 6 | Not yet valid | "This key isn't active yet." |
| 7 | Door has no public key yet | "The door isn't ready yet. Try again in a minute." |
| 8 | Busy or rate limited | "The door is busy. Try again." |

## 4. What the door checks, in order

1. The request belongs to the live connection and its nonce is unexpired (10 s) and unused.
2. Framing and sizes.
3. `jwtVerify`: the RS256 signature first, then claims. `aud` is exactly `"ble"`; `cnf` decodes to
   65 bytes starting `0x04` and is a valid curve point.
4. The ECDSA signature against `cnf`, over the door's **own** door id and the live nonce.
5. The tag rules minus the serial check: blacklist by `jti`, `exp`, `nbf` (with
   `kNotBeforeLeeway`), `iat` (with `kIssuedAtLeeway`, when the clock is trusted).

The nonce is destroyed when a request is answered (any outcome), on disconnect, and on timeout.
After a failed attempt the door answers code 8 to every request for 2 s.

## 5. Invariants (each has a test in the door's host tests)

- A token with `aud: "ble"` never opens the door from an NFC tag; a tag token never opens it over BLE.
- No request without a valid proof opens the door, whatever its token says.
- A nonce is accepted once; a recorded request replayed on a new connection fails.
- A signature made for another door id, or over another nonce, fails.
- Nothing from the bridge is trusted before the RS256 signature has been checked, and nothing is
  parsed with an unbounded length.
- Only the door's `Actuators` drives the relay.

Accepted, not prevented: a relay attack (someone forwarding the radio link from a phone that is
elsewhere); the user's tap on "Unlock" is the mitigation. The JWT travels in clear, so a listener
nearby learns a user id and key id; without the phone's private key that is useless.

## 6. UART link (bridge ⇄ door)

115200 baud, 8N1, no flow control, 3.3 V logic, common ground.

Each frame is COBS-encoded and ends with `0x00`. Decoded frame:

```
type (1) | conn (1) | payload (0..1100) | crc16 (2)
```

`crc16` is CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF; `crc16()` in `doora/src/Crc.h`) over
`type | conn | payload`. A frame with a bad CRC, an unknown type or an oversized payload is
dropped and counted. `conn` is a connection number the bridge assigns per BLE connection
(1..255, wrapping, never 0); messages not about a connection use 0. The door ignores any message
for a connection other than the current one.

| Type | Direction | Payload |
| --- | --- | --- |
| `0x10 HELLO` | bridge → door | link version (1) = 0x01, bridge firmware version (ASCII, ≤ 32) |
| `0x11 PING` | door → bridge | none; every 5 s |
| `0x12 PONG` | bridge → door | none |
| `0x20 CONNECTED` | bridge → door | none; the phone has subscribed and waits for a challenge |
| `0x21 CHALLENGE` | door → bridge | a challenge message (section 3), notified verbatim |
| `0x22 REQUEST` | bridge → door | a reassembled request body (section 3) |
| `0x23 RESULT` | door → bridge | a result message, notified verbatim, then the bridge disconnects |
| `0x24 GONE` | bridge → door | none; the phone disconnected or timed out |

The bridge sends `HELLO` at boot and in reply to the first `PING` after a gap. The door treats the
bridge as offline after 3 missed `PONG`s and shows that in `status`. A link version it does not
know is logged and the bridge ignored.

## 7. Test vectors

`docs/ble-test-vectors.json` holds fixed inputs and the exact bytes they produce: a P-256 key
pair, a door id, a nonce, the signed message, a DER and a raw signature, a credential, the chunked
writes at MTU 23, 185 and 247, a request body, and UART frames before and after COBS. The door's
host tests and the app's unit tests both read this file, so the implementations cannot drift.
