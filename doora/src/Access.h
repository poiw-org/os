// The door's single access decision, free of I/O so every branch is unit tested.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Jwt.h"

enum class Verdict : uint8_t { Allow, SerialMismatch, Blacklisted, Expired, NotYetActive, IssuedInFuture };

// Tolerance for the clock difference between the backoffice that issued the key and the door.
constexpr uint32_t kNotBeforeLeeway = 30;
constexpr uint32_t kIssuedAtLeeway = 300;

// `clockTrusted` is false when `now` is only an estimate (no NTP/server sync since boot).
// Then "issued in the future" is not checked, because an estimate can lag real time.
Verdict decide(const KeyClaims& key, const uint8_t* uid, size_t uidLen, uint32_t now, bool clockTrusted,
               bool blacklisted);

// Value for entries.reason; Allow has none.
const char* reasonName(Verdict v);
