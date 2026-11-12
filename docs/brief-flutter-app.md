# Brief: the Doora phone app (Flutter, iOS and Android)

You are building the phone app for Doora, the door lock of the po/iw hackerspace. A member signs
in, sees their virtual keys, and when the phone is at the door the app offers to unlock it. The
unlock works with the phone offline and the door offline: the door checks everything locally.

Read before you start: `docs/ble-protocol.md` (normative; you implement the phone side exactly),
`README.md`, `web/README.md`, `web/design.md`, `web/src/app.css`, `web/src/lib/console.css`, and
the web code named below. Another agent builds the door, bridge and backend from
`docs/brief-door-bridge-web.md`; you depend on its `ble-credential` function and its
`docs/ble-test-vectors.json`. If those do not exist yet, build against the protocol and a fake,
and say so in your report.

Quality bar: this is the app members will open at a locked door. It must be fast, never confusing,
and never leave someone outside because of a state the app did not handle.

## 1. Scope

In: sign-in (with the second step the account requires), the member's virtual keys, setting up
this phone for a key, finding the door, the unlock prompt and the unlock itself, background
prompts, sign-out.

Out: admin features, issuing or renewing keys, remote (internet) unlock, onboarding, registering
passkeys or authenticator apps, NFC. Where a member needs one of these, say in one sentence that
it is done on the website (`https://poiw.org`) and link to it.

## 2. Platform and packages

- `app/` at the repository root. Flutter stable, Dart 3, null-safe. iOS 15+, Android 8.0+ (API 26).
  Bundle id `org.poiw.doora` unless the owner says otherwise.
- No state-management framework and no code generation: `ChangeNotifier`,
  `ValueListenableBuilder` and plain classes are enough for three screens.
- Expected packages: `supabase_flutter`, `flutter_blue_plus`, `flutter_local_notifications`,
  `flutter_secure_storage`. **Check each one's current version, platform support and API on
  pub.dev before using it; do not rely on remembered APIs.** Add nothing else without a reason you
  can state in the report.
- `SUPABASE_URL` and `SUPABASE_PUBLISHABLE_KEY` come from `--dart-define` (the values are the web
  app's `PUBLIC_SUPABASE_*` in `web/.env.production`). Do not commit them in Dart code. Provide a
  `make`/script target that passes them.

```
app/lib/
  main.dart                   start-up, Supabase.initialize, routing by auth state
  config.dart                 dart-define values
  theme/                      tokens, text styles, the few shared widgets (section 3)
  auth/                       sign-in screen, second step, session state
  keys/                       key model, key list screen, credential store, status logic
  door/
    ble_protocol.dart         pure Dart: UUIDs, messages, chunking, DER → raw, signed bytes
    door_scanner.dart         scanning and proximity
    door_unlocker.dart        connect, challenge, sign, write, result
    unlock_sheet.dart         the prompt
  platform/device_key.dart    MethodChannel wrapper
app/ios/Runner/DeviceKey.swift
app/android/app/src/main/kotlin/.../DeviceKey.kt
```

## 3. Design: match the web app

The app must look like it belongs to the website. **`web/src/app.css` and
`web/src/lib/console.css` are the source of truth**; `web/design.md` explains the intent but is
partly stale (it says Space Mono everywhere and 4 px corners; the CSS uses Space Grotesk for
headings and body, Space Mono for labels and buttons, and square corners). Open the reference
screenshots in `web/e2e/baseline/` (`login-mobile.png`, `home-mobile.png`, `key-detail-mobile.png`)
and run the web app (`pnpm dev` in `web/`) on a phone-sized viewport to compare side by side.

**Tokens.** Convert every `oklch()` value in `app.css` to sRGB with a script (for example
`culori`), commit the script, and generate one Dart file of named colours. Do not eyeball colours.
Names follow the CSS: `backing`, `void`, `paper`, `paper2`, `ink`, `gray200` (hairline), `gray300`,
`gray400` (muted), `gray600` (secondary ink), `brand`, `brandHover`, `brandText`, `green700`,
`red500`, `yellow500`, `blue500`, and the inverted set under `.light, dialog.sheet`.

**Type.** Bundle Space Grotesk (variable) and Space Mono 400/700 as assets (both OFL; include the
licences). Do not fetch fonts at runtime: the app is used offline. Headings Space Grotesk bold,
-0.01em tracking. Body Space Grotesk 14 at 1.5 line height. Labels (`.eyebrow`) Space Mono 11,
uppercase, 0.14em tracking, `gray400`. Buttons Space Mono 11, uppercase, 0.12em tracking. Respect
the system text-size setting; layouts must survive 200 %.

**Surfaces and components** (rebuild these as small widgets, not a Material theme with tweaks):

- Background `void`, dark only, no gradients, no shadows, no blur.
- Square corners everywhere (radius 0).
- `.panel`: `paper` fill, 1 px `gray200` border, a 1 px inner highlight on the top and left
  (white at 10 %), children separated by 1 px hairlines.
- `.row`: 10 × 16 padding, content spread across the row.
- `.btn`: `paper2` fill, 1 px `gray200` border, bevel (inner 1 px white 14 % top-left, black 25 %
  bottom-right), `gray600` text; pressed moves down 1 px with an inset shadow. `.btn-primary` is
  `brand` with white text. `.tone-red` only for destructive actions. One primary button per screen.
- Inputs: `backing` fill, 1 px `gray200` border, inset shadow, `brandText` border and a 2 px
  `brand` ring at 55 % on focus.
- Key status: the web's `KeyStatusBadge`: a "readout chip" (dark recessed box, mono caps) with a
  lamp (8 px square, 1 px corner, soft glow in its colour) and the label. Lamps and labels exactly
  as `web/src/lib/status.ts` and `KeyStatusBadge.svelte`: green `ENABLED`, amber `EXPIRED`, blue
  breathing `NOT YET ACTIVE`, red unlit `DISABLED`.
- The `VIRTUAL` tag from `VirtualBadge.svelte` (`.tag.tag-signal`) where a key is shown.
- Focus: 2 px `brandText` outline, 2 px offset.

**Rules carried over from `design.md`:** no icon library (actions are words; status is a lamp plus
text); no pills, tinted alert boxes or side stripes; sentence-case headings, no subtitle under every
heading; loading is the word "Loading…", not a spinner; avatars, if any, are Gravatar or a flat
hue.

**Motion:** durations 120 ms (colour, press), 220 ms (rows, sheets), 360 ms (screens); easing
`cubic-bezier(0.23, 1, 0.32, 1)`. Status text swaps in place. Nothing loops except the blue lamp's
breathing and the progress state during an unlock. Under the system's reduce-motion setting,
everything becomes an instant change. No 3D scenes.

## 4. Screens

### 4.1 Sign-in

A single left-aligned column on `void` with a `.panel` holding the form, like the web's
`AuthShell.svelte` and `routes/login/+page.svelte`. Brand at the top (`web/static/poiw.png`,
recoloured as in `Brand.svelte`).

Mirror `web/src/lib/auth.ts` and the database rules it serves (migrations `20261108120000`,
`20261108130000`, `20261108140000`, `20261109160544`):

- Email and password (`signInWithPassword`).
- Then the second step the account requires, decided the way the web decides `needsCode`: an
  authenticator code (`auth.mfa.challengeAndVerify`) or the emailed code (read
  `web/supabase/functions/confirm-email-code` and the web flow that calls it, and do the same).
- Passkey-only accounts: passkey sign-in is not in this version. Say plainly that the account needs
  a password and point to the website. Report this as a gap.
- Errors in one calm sentence under the field they concern. Disable the button while a request
  runs and show "Signing in…".

After sign-in, call the `can_use_app` RPC. If false: one screen saying "You need an active virtual
key to use the app. Ask an admin." and a "Sign out" button. Nothing else.

### 4.2 Keys

The home screen. Heading "Keys", then a `.panel` with one `.row` per virtual key
(`keys` where `serial_number is null`; RLS returns only the member's own), sorted active first,
then by expiry. Each row: the status chip, the validity dates in the web's format
(`web/src/lib/format.ts`), the `VIRTUAL` tag, and one of:

- **Not set up:** a primary "Set up this phone" button. It creates this key's key pair, calls
  `ble-credential` with the public key and the device name (the user-visible device name; ask for
  nothing), and stores the returned credential.
- **Set up on this phone:** "Ready" and, when the app is in the foreground and near the door, an
  "Unlock" button as well (the sheet is the main path; this is the fallback).
- **Set up on another phone:** the device name and "Bindings are permanent. Ask an admin for a new
  key to use this phone."
- **Disabled, expired or not yet active:** the status only; no actions.

Below the list: a line with the signed-in email and a "Sign out" button. Pull to refresh. Data
loads from cache first, then the network; offline, the list shows what is cached with
"Offline: showing saved keys" and everything still works for unlocking.

### 4.3 The unlock prompt

The owner wants something like the AirPods pairing card. **No app can show a system card over the
home screen on iOS or Android; that card is private to Apple.** Build the closest honest version:

- **App in the foreground:** when the door is near (section 6), present a sheet from the bottom
  using the web's inverted light surface (`.light, dialog.sheet` tokens): square corners, a drag
  handle, the door's name as the heading, the key that will be used as one detail line, one
  primary "Unlock" button and a "Not now" text button. Drag down, "Not now", or a tap on the scrim
  dismisses it. After a dismissal, do not prompt for that door again for 60 s. Use the platform's
  sheet mechanics (`showModalBottomSheet` with `useSafeArea`; on iOS match the system's drag
  resistance and dismiss velocity), not its visual style.
- **During the unlock:** the button becomes "Unlocking…" and the sheet cannot be dismissed by
  accident. On success: "Door open" with the green lamp, a success haptic, and the sheet closes
  after 1.5 s. On failure: the door's sentence from protocol section 3 and "Try again" where a
  retry makes sense (codes 1, 7, 8, and connection errors), "Close" otherwise.
- **App in the background:** a time-sensitive local notification, "You're at the door", with an
  "Unlock" action. The action opens the app and runs the unlock at once; the tap is the consent.
  Never unlock without a user action. A notification action that unlocks without opening the app
  is a later refinement; only do it if it proves reliable on real devices on both platforms.
- If several keys are ready, use the one that expires last and name it in the sheet.

## 5. Device key and credentials

Platform channel `org.poiw.doora/device_key`:

| Method | Returns |
| --- | --- |
| `publicKey(keyId)` | 65 bytes, `04 || X || Y`; creates the pair on first use |
| `sign(keyId, message)` | the DER signature |
| `delete(keyId)` | nothing |

One pair per key id, because `device_pubkey` is unique across keys.

- **iOS:** `SecKeyCreateRandomKey`, `kSecAttrKeyTypeECSECPrimeRandom`, 256 bits,
  `kSecAttrTokenIDSecureEnclave`, permanent, application tag `org.poiw.doora.<keyId>`, access
  control `kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly` with `.privateKeyUsage` and no
  biometry flag, so signing works after a notification tap. Public key from
  `SecKeyCopyExternalRepresentation`. Sign with `.ecdsaSignatureMessageX962SHA256`. The simulator
  has no Secure Enclave: a software key in debug builds only, never in release.
- **Android:** `KeyPairGenerator` on `AndroidKeyStore`, `secp256r1`, `PURPOSE_SIGN`,
  `DIGEST_SHA256`, StrongBox when available with a fallback, no user-authentication requirement.
  Build the 65-byte point from the certificate's public key with X and Y left-padded to 32 bytes.
  Sign with `SHA256withECDSA`.
- In Dart, convert DER to raw `r || s`: strip the DER sign byte, left-pad each to 32 bytes.
- The private key never leaves the keystore. Never log keys, signatures or credentials, in debug
  builds either.

Credentials:

- Stored in `flutter_secure_storage` per key id, with `not_before` and `expires_at`.
- On launch and on resume, when online: list keys, call `ble-credential` again for keys set up on
  this phone (the same public key returns the current credential, which is how renewals arrive),
  and drop stored credentials for keys that are disabled, expired or gone.
- Offline: use what is stored and never sign the user out for being offline.
- Signing out deletes stored credentials. It deletes the key pairs only after a confirmation that
  says a deleted pair can never be set up again for that key.

## 6. Finding the door

- Scan with a filter on the service UUID from the protocol. The door is "near" when RSSI is above
  a threshold for two consecutive results; start at -75 dBm, keep it one named constant, and tune
  it at the real door (the bridge's antenna is weak).
- Scan only while there is at least one usable credential, and stop scanning when the app has
  none. In the foreground scan continuously; in the background use the platform mechanism below.
- **iOS:** `UIBackgroundModes` `bluetooth-central`, scan with the service UUID, Core Bluetooth
  state restoration so iOS relaunches the app when the door is seen. Measure background discovery
  time and report it.
- **Android:** background discovery needs a `PendingIntent` scan (a small Kotlin
  `BroadcastReceiver`) or a foreground service. Prefer the `PendingIntent` scan; write it natively
  if `flutter_blue_plus` does not expose it. Respect Doze; measure and report discovery time.
- The door's display name comes from `public.doors` when cached, else the door id from the
  challenge.

## 7. Unlock sequence

Exactly as the protocol: connect (5 s timeout); on Android request MTU 247; discover the
characteristic; subscribe; wait for the challenge (check type and version); build the signed
message with the challenge's door id and nonce; sign with this key's pair; send the request in
chunks sized to the negotiated MTU (MTU − 3 − 1 header byte), each with response; wait for the
result; disconnect. Whole operation at most 10 s. One automatic retry on a connection-level
failure, never after the door has answered. Always disconnect, whatever happens.

## 8. Permissions

- **iOS `Info.plist`:** `NSBluetoothAlwaysUsageDescription` ("Doora uses Bluetooth to find the
  door and unlock it."), `UIBackgroundModes` `bluetooth-central`. Ask for notification permission
  when the user turns on background prompts, not at launch.
- **Android:** `BLUETOOTH_SCAN` with `android:usesPermissionFlags="neverForLocation"`,
  `BLUETOOTH_CONNECT`, `POST_NOTIFICATIONS` (API 33+); for API ≤ 30 `BLUETOOTH`,
  `BLUETOOTH_ADMIN` and `ACCESS_FINE_LOCATION` with `android:maxSdkVersion="30"`.
- Before each system dialog, one short screen in the app's style saying why. If a permission is
  denied, show what no longer works and a button to the system settings. No dead ends: Bluetooth
  off, permission denied, no keys, offline, and door out of range each have a clear state.

## 9. Accessibility

VoiceOver and TalkBack labels on every control and status ("Key, enabled, valid until 3 March").
Minimum 44 × 44 pt touch targets. Contrast at least WCAG AA for text (check the converted tokens).
Text scaling to 200 %. Reduce-motion respected. The unlock sheet is announced when it appears.

## 10. Tests

- **Unit:** `ble_protocol.dart` against `docs/ble-test-vectors.json` (the same file the door's tests
  read): challenge parsing, signed-message bytes, DER to raw (short and long integers, leading
  zeros), chunking at MTU 23, 185 and 247, result codes to sentences. Key status logic against
  `web/src/lib/status.ts`'s rules.
- **Widget:** sign-in (both second steps, errors), the "no virtual key" screen, the key list in
  each row state, offline, and the sheet in each phase.
- **Fake door:** an in-process fake implementing the door's side from the protocol, so the unlock
  flow is tested without hardware, including timeouts, a bad nonce and every result code.
- **Real devices:** a real iPhone and a real Android phone each unlock the bench door (the other
  agent's test setup) with Wi-Fi off on the phone and the door. Background prompts tested with the
  app killed and with the phone locked. Report timings.
- `flutter analyze` clean, `dart format` applied, `flutter test` passing.

## 11. Rules

- Do not change anything outside `app/` and `docs/` except to read it. If the protocol or the
  `ble-credential` function needs a change, stop and say what and why.
- Never commit secrets, signing material, real user data or tokens.
- Commit in small steps in the repository's style (`git log`). Do not push or open a PR unless
  asked. Do not publish to the App Store or Play Store.
- Report plainly: what works on which device, measured discovery and unlock times, anything not
  tested, every gap (passkey sign-in, anything else).
