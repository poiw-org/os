# Doora door controller (ESP32)

Firmware for the door lock: a PN532 NFC reader, a relay, a buzzer and a few LEDs. A tap is decided
**on the device**, so the door keeps working through Wi-Fi, internet and power outages. Supabase is
used to keep the device's local state fresh and to report what happened.

## How it works

```
tag ──► read token ──► verify HS256 signature ──► serial matches? blacklisted? expired? not yet valid?
                        (secret from EEPROM)        (clock + blacklist from flash)
                                    │
                              open / deny ──► event appended to a flash journal ──► uploaded when online
```

- **Key tokens.** The `keys` edge function signs a JWT (`sub` user, `jti` key id, `aud` tag serial,
  `iat`/`nbf`/`exp`) that is written to the tag. The door verifies it offline with the shared secret.
- **Blacklist.** Disabled keys. Fetched in full from `public.door_sync()` on every (re)connect and
  every 15 minutes, updated live from realtime changes on `public.keys`, and mirrored to flash. Entries
  drop out once the key would have expired anyway. Up to 200 keys.
- **Clock.** SNTP, or the server time from `door_sync()` when NTP is blocked. A checkpoint is written
  to flash every 10 minutes. After a blackout the door starts from the newest of that checkpoint and
  the firmware build time and counts up from there. That estimate can only lag real time.
- **Journal.** Every tap, allowed or denied, and every force open is appended to a flash queue (256
  events) and uploaded in batches through `public.door_log()`. Events are removed only after the server
  acknowledged them; the server ignores ones it already has, so a lost reply is harmless.
- **Realtime.** One websocket on the private channel `door:<id>`: key changes (postgres_changes) and
  `force_open` commands (broadcast). The door answers `force_open_ack` so the backoffice can tell the
  user the door really unlocked. Commands are never stored, so one can never fire after an outage.
- **Account.** The door signs in as a normal Supabase user listed in `public.door_users`. It can read
  keys, call `door_sync()` and `door_log()`, and use its own channel. It cannot read entries, change keys
  or see other tables.

## What happens when something fails

| Failure                          | Behaviour                                                                                            |
| -------------------------------- | ---------------------------------------------------------------------------------------------------- |
| No Wi-Fi / internet / Supabase   | Taps are decided locally; events queue in flash; retries with exponential backoff (3 s to 5 min).     |
| Power cut                        | Blacklist, clock checkpoint and journal are restored from flash. Writes are atomic or checksummed.     |
| Power cut during a write         | Files are replaced via temp file + rename; a torn journal record is detected and repaired at boot.     |
| Blacklist file corrupt           | Treated as empty and refetched at the next connection. Until then disabled keys still open the door.   |
| Missed realtime event            | The 15-minute resync and every reconnect reconcile against the server.                                |
| Half-open connection             | Websocket heartbeat every 25 s; a missing reply forces a reconnect.                                  |
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
- One TLS session is used at a time to keep memory use flat, so uploads and token refreshes happen in
  short maintenance windows (a few seconds, only when nobody has used the door for 4 s) during which
  the websocket is closed and reopened.

## Set up

### 1. Supabase

The migrations in `backoffice/supabase/migrations` create the door role, its policies, the realtime
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
| Key signing secret           | Exactly the `JWT_SIGNING_KEY` secret of the `keys` edge function            |
| NTP server                   | `pool.ntp.org`                                                              |

### Console commands

Type these in the serial monitor at any time: `status`, `show` (secrets are hidden), `config` (change
settings; Enter keeps the current value), `erase [wifi|supabase|door|ntp|all]` (partial erase keeps the rest), `reboot`.

TLS to Supabase is always verified against the roots in `src/Certs.h`. If Supabase ever changes
certificate authority, add the new root there and reflash; until then the door runs offline.

## Tests

The access decision, token verification, blacklist, sync parser, journal format and time helpers are
portable C++ and are tested on the workstation:

```sh
make test   # builds host_test/ with the system C++ compiler
```
