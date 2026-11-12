# Doora door controller (ESP32)

Firmware for the door lock, for an ESP32 with a PN532 NFC reader, a relay, a passive buzzer and a few
LEDs. A tap is decided on the device, so the door keeps working through Wi-Fi, internet and power
outages. Supabase is only used to keep the door's local state fresh and to report what happened.

## Source layout

```
src/            firmware
  main.cpp        setup and the main loop (taps, relay, console)
  Access.*        the open/deny decision
  Jwt.*, Rsa.*, Sha256.*   token parsing and signature checks
  BlacklistSet.*, SyncParser.*   disabled keys and the sync response
  Journal.*, Persist.*     event queue and files in flash
  Clock.*, TimeUtil.*      time from NTP, the server, or a flash checkpoint
  Pn532*.*, SoftI2c.*, Reader.*, Ndef.*   NFC reader driver, tag reading and writing
  Cloud.*, Net.*, Realtime.*, WsClient.*, WsFrame.*   Supabase sign-in, REST, realtime websocket
  Actuators.*, Chimes.h    relay, LEDs, buzzer tones
  Settings.*, Console.*    EEPROM config and the serial console
  Bt.*, BtGate.h, Term.h   the console over Bluetooth, gated by admin approval
  Ota.*, OtaImage.*        firmware updates, approved in the portal
host_test/      unit tests for the portable parts, run on the computer with `make test`
tools/          release.py publishes a build for approval
config.example.h   pin template; copy it to config.h
platformio.ini  build environment (esp32dev) and partition table
```

## How it works

```
tag ──► read token ──► verify signature (v2: RS256 with the public key, v1: HS256) ──► serial matches? blacklisted? expired? not yet valid?
                        (secret from EEPROM)        (clock + blacklist from flash)
                                    │
                              open / deny ──► event appended to a flash journal ──► uploaded when online
```

- **Key tokens.** The `keys` edge function signs a JWT (`sub` user, `jti` key id, `aud` tag serial,
  `iat`/`nbf`/`exp`) that is written to the tag. The door verifies it offline with the shared secret.
- **Blacklist.** Disabled keys. Fetched in full from `public.door_sync()` at start-up, after every
  realtime reconnect (at most every 30 s) and every 5 minutes, updated live from realtime changes on
  `public.keys`, and mirrored to flash. A key from the v1 system is blocked under both its ids, because
  its card may carry either as the token's `jti`. Entries drop out once the key would have expired
  anyway. Up to 200 keys.
- **Clock.** SNTP, or the server time from `door_sync()` when NTP is blocked. A checkpoint is written
  to flash every 10 minutes. After a blackout the door starts from the newest of that checkpoint and
  the firmware build time and counts up from there. That estimate can only lag real time.
- **Journal.** Every tap, allowed or denied, and every force open is appended to a flash queue (256
  events) and uploaded in batches through `public.door_log()`. Events are removed only after the server
  acknowledged them; the server ignores ones it already has, so a lost reply is harmless.
- **Realtime.** One websocket on the private channel `door:<id>`: key changes (postgres_changes) and
  `force_open` commands (broadcast). The door answers `force_open_ack` so the backoffice can tell the
  user the door really unlocked, and answers a repeated command again without opening twice. Commands
  are never stored, so one can never fire after an outage. The connection has a task of its own that
  sleeps until a frame arrives, so a command reaches the relay within milliseconds, and it is never
  closed for upkeep: the access token is renewed on the open channel.
- **Writer mode.** The backoffice's "Write at door" sends `write_card` with a key token. The door only
  accepts a token it can verify with its own signing secret, then waits 60 s for the tag named in the
  token's audience (no tap opens the door meanwhile), writes it as an NDEF text record, reads it back
  and reports `write_card_status` (`ready`, `wrong_tag`, `writing`, `done` or a failure). NFC Forum
  Type 2 tags (NTAG213 is too small for most keys; use NTAG215/216) and MIFARE Classic 1K cards
  (NDEF-formatted ones are written with key B, the factory default, else key A; a blank card with
  factory keys is formatted automatically first: MAD plus the NDEF key A, which erases any data on it;
  a card with other keys is refused untouched). It beeps the attention tone
  while waiting, a faster tone while writing, and the happy chord when done.
- **Account.** The door signs in as a normal Supabase user listed in `public.door_users`. It can read
  keys, call `door_sync()` and `door_log()`, and use its own channel. It cannot read entries, change keys
  or see other tables.

## What happens when something fails

| Failure                          | Behaviour                                                                                            |
| -------------------------------- | ---------------------------------------------------------------------------------------------------- |
| No Wi-Fi / internet / Supabase   | Taps are decided locally; events queue in flash. Realtime retries after 1 s, backing off to 30 s; uploads and sign-in back off to 5 min. |
| Power cut                        | Blacklist, clock checkpoint and journal are restored from flash. Writes are atomic or checksummed.     |
| Power cut during a write         | Files are replaced via temp file + rename; a torn journal record is detected and repaired at boot.     |
| Blacklist file corrupt           | Treated as empty and refetched at the next connection. Until then disabled keys still open the door.   |
| Missed realtime event            | Every reconnect and a resync every 5 minutes reconcile against the server.                           |
| Half-open connection             | Websocket heartbeat every 10 s; no reply within 4 s forces a reconnect.                              |
| Token expired / revoked          | Refreshed at 75 % of its lifetime; falls back to signing in again.                                   |
| Server rejects a batch of events | A permanently unacceptable batch (HTTP 400/413/422) is dropped so it cannot block the queue.         |
| Door account misconfigured       | Logged on the serial console; retries every 5 minutes. The door itself keeps working.                |
| Memory pressure / fragmentation  | TLS is skipped when memory is too low; the device restarts itself when idle if that persists.         |
| NFC reader stops answering       | Re-initialised; restarted if it keeps failing.                                                       |

Known limits, by design of the hardware or the protocol:

- **Offline, a disabled key still opens the door** until the door learns about it. Disabling is only
  as fast as the door's connection.
- **A deleted key is not blocked.** Disable it first, then delete it (the backoffice warns about this).
- **Estimated clock.** After a long blackout without network the clock lags: an expired key can
  still work and a freshly activated one can be refused until the first sync.
- Two TLS sessions are open while the door uploads or renews something (about 80 KB of heap). That is
  what lets the websocket stay up during upkeep. If memory is too low for a second session, the
  websocket closes for the seconds the upload takes and reconnects afterwards; `status` counts how
  often. A force open sent in that gap is not received, and a key disabled in it is picked up by the
  sync that ends the gap or, at worst, by the next periodic one (5 minutes).

The firmware runs as four tasks: `loop()` (taps, relay, console), the NFC reader, the realtime
websocket and the REST work (sign-in, event upload, blacklist sync). They share only a few atomic flags
and three locked objects (the blacklist, the access token and the flash files), and no task waits on a
network call made by another. `status` shows each task's stack headroom and the lowest free heap seen.

## Set up

### 1. Supabase

The migrations in `web/supabase/migrations` create the door role, its policies, the realtime
channel policies and the `door_sync` / `door_log` functions.

Create the door's account in the dashboard (Authentication > Users > Add user, auto-confirm), then
bind it to a door:

```sql
insert into public.door_users (user_id, door_id)
select id, 'main' from auth.users where email = 'door@example.com';
```

Revoke a door at any time with `delete from public.door_users where ...` or by banning the user.

### 2. Wiring

```sh
cp config.example.h config.h   # then edit the pins (GPIO numbers)
```

The speaker must be a **passive** buzzer (or piezo): the firmware plays tones on it. A rolled major chord
sounds while the door is unlocked, a falling minor chord when a key is denied, and a force open from the
web UI sounds an attention tone for 3 seconds before the relay fires.

`config.h` holds wiring only and is git-ignored. The build stops if `DOOR_PIN` or `SPEAKER` is missing,
because guessing a relay pin could fire the lock. The PN532 reader (in I2C mode) is wired to SDA 21
and SCL 22; `NFC_SDA` and `NFC_SCL` in `config.h` change that. The firmware drives the chip with its
own small driver (`Pn532.cpp`, over a bit-banged I2C bus), so there are no NFC libraries to install.

### 3. Build and flash

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/) (or use the VS Code
extension). The first build downloads the ESP32 toolchain and ArduinoJson.

```sh
pio run                # build
pio run -t upload      # flash
pio device monitor     # console, 115200 baud
```

`make build|upload|monitor|test` are shortcuts for the same commands. The board is `esp32dev`; change
`board` in `platformio.ini` for other ESP32 modules.

#### The console over Bluetooth

The door shows up as a Bluetooth Classic serial device named `doora-<door id>`. Pair with it, then
open its serial port (on a Mac, `/dev/cu.doora-<door id>`). Commands and logs are the same as on USB.

- **An admin must approve every new device.** A prompt with the device's address appears for admins
  signed in to the webapp. Without an approval within 20 s the connection is dropped, and so it is
  when the door is offline.
- **An approved device stays authorized for 24 hours** from the approval and is then cut off. A
  restart of the door or a new pairing ends the authorization early.
- Pairing has no PIN (the door has no screen), so it is the approval that keeps strangers out.
- First-run setup is USB only, because the door needs its settings to reach the webapp. For the same
  reason `erase` over Bluetooth can lock Bluetooth out until someone runs setup over USB.

#### Firmware updates

The door has no listener. `make release` uploads the build to Supabase and opens its approval page.
An admin approves that exact hash (valid for 30 minutes), and the door downloads it over its own
connection and installs it only if it matches the hash.

```sh
pio run -t upload                    # once over USB, so the door has the update code
make release                         # from then on
```

Builds are not signed, so the door trusts what its Supabase project approves: whoever controls that
project can install firmware.

Admins and members with an active key can publish builds; only admins approve them. The first
`make release` opens the portal to ask for permission. The resulting token lasts 30 days, is kept in
`~/.doora/release_token`, can only queue builds for approval, and stops working if its owner no longer
qualifies. Rotating the function secret `FIRMWARE_CLI_JWT_SECRET` revokes
all of them. `firmware-upload` and `firmware-cli-auth` are deployed with `--no-verify-jwt`.

- **A new image runs on trial.** If the door resets before it is back on the cloud, or is not back
  within 10 minutes, the boot loader returns to the previous firmware and the portal says so.

### 4. First boot

With no stored configuration the door asks for everything over serial (115200 baud) and stores it in
EEPROM. It restarts when you save.

| Question                     | Value                                                                       |
| ---------------------------- | --------------------------------------------------------------------------- |
| Wi-Fi name and password      | Your network (empty password for an open network)                           |
| Supabase host                | `<project>.supabase.co`                                                     |
| Supabase publishable key     | Project Settings > API Keys (`sb_publishable_...` or the legacy anon key)   |
| Door account email, password | The user you created above                                                  |
| Door id                      | `main` unless you run several doors                                         |
| Key signing secret           | The old `JWT_SIGNING_KEY`; only for v1 (HS256) keys. Optional: empty refuses v1 keys |
| NTP server                   | `pool.ntp.org`                                                              |

### Console commands

Type these in the serial monitor at any time: `status`, `show` (secrets are hidden), `config` (change
settings; Enter keeps the current value), `erase [wifi|supabase|door|ntp|all]` (partial erase keeps the rest), `reboot`.

TLS to Supabase is always verified against the roots in `src/Certs.h`. If Supabase ever changes
certificate authority, add the new root there and reflash; until then the door runs offline.

## Tests

The access decision, token verification, blacklist, sync parser, journal format and time helpers, the
websocket framing, the retry backoff, the PN532 driver (against a simulated chip) and the flash
journal (against an in-memory filesystem) are portable C++ and are tested on the workstation:

```sh
make test   # builds host_test/ with the system C++ compiler
```

## Key versions

- **v1** keys are HS256 tokens signed with the shared `JWT_SIGNING_KEY`, checked with the signing secret stored on the door.
  Anyone can read a v1 tag and guess a weak secret offline, so clear the secret (`config`, then `-`) once every v1 tag
  has been reissued as v2: the door then refuses HS256 tokens.
- **v2** keys are RS256 tokens. The backoffice derives the key pair from its `RSA_KEY_SEED` secret each time it signs, and the door
  fetches only the public modulus from the `door-key` edge function after signing in. It is kept in flash, so v2 keys verify offline
  after the first fetch (it is refreshed every 6 hours). Until it has arrived, a v2 tag is denied with a log line saying so.

The door accepts both. The backoffice issues only v2 keys.
