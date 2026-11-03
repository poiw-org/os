// RSASSA-PKCS1-v1_5 / SHA-256 signature verification with a 2048-bit public key and e = 65537, which
// is all the door needs to check a v2 key token. Portable and allocation-free, so the same code
// runs on the ESP32 and in the host tests.
#pragma once
#include <stddef.h>
#include <stdint.h>

constexpr size_t kRsaBytes = 256;  // modulus and signature size

// `n` is the big-endian modulus, `sig` the big-endian signature and `hash` the SHA-256 of the
// signed text. False for a bad signature and for a modulus that is not a 2048-bit odd number.
bool rsaVerifySha256(const uint8_t n[kRsaBytes], const uint8_t sig[kRsaBytes], const uint8_t hash[32]);
