// The door's single access decision, free of I/O so every branch is unit tested.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "BleProto.h"
#include "Jwt.h"

enum class Verdict : uint8_t { Allow, SerialMismatch, Blacklisted, Expired, NotYetActive, IssuedInFuture };

// Tolerance for the clock difference between the backoffice that issued the key and the door.
constexpr uint32_t kNotBeforeLeeway = 30;
constexpr uint32_t kIssuedAtLeeway = 300;

// `clockTrusted` is false when `now` is only an estimate (no NTP/server sync since boot).
// Then "issued in the future" is not checked, because an estimate can lag real time.
Verdict decide(const KeyClaims& key, const uint8_t* uid, size_t uidLen, uint32_t now, bool clockTrusted,
               bool blacklisted);

// The decision for a phone over BLE, from the credential's claims (already verified by jwtVerify),
// the signature the phone made over the door's own id and the live nonce, and the same clock and
// blacklist as a tap. Checks in the order of docs/ble-protocol.md section 4: the audience, the
// public key and the proof first, then the rules every key shares. A tag token fails here and a
// BLE credential fails in decide(), so neither can be used on the other path.
BleResult decideBle(const KeyClaims& key, const char* doorId, const uint8_t nonce[kBleNonceLen],
                    const uint8_t sig[kBleSigLen], uint32_t now, bool clockTrusted, bool blacklisted);

// What a refused BLE attempt is journalled as: the Verdict of a rule every key shares, or 255 when no
// Verdict applies (a bad credential or proof), which is stored as reason "Unknown".
uint8_t journalVerdict(BleResult r);

// Value for entries.reason; Allow has none.
const char* reasonName(Verdict v);
