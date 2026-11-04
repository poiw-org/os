// RSASSA-PKCS1-v1_5 / SHA-256 signature verification with a 2048-bit public key and e = 65537, which
// is all the door needs to check a v2 key token. Portable and allocation-free, so the same code
// runs on the ESP32 and in the host tests.
#pragma once
#include <stddef.h>
#include <stdint.h>

constexpr size_t kRsaBytes = 256;  // modulus and signature size

// A public key ready for verification. Preparing it costs about as much as a verification itself,
// so it is done once when the key arrives instead of on every tap.
struct RsaPublicKey {
	uint32_t n[kRsaBytes / 4];   // modulus, least significant limb first
	uint32_t r2[kRsaBytes / 4];  // 2^4096 mod n, to enter Montgomery form
	uint32_t n0inv;              // -n^-1 mod 2^32
};

// `n` is the big-endian modulus. False when it is not a 2048-bit odd number.
bool rsaPrepare(const uint8_t n[kRsaBytes], RsaPublicKey& key);

// `sig` is the big-endian signature and `hash` the SHA-256 of the signed text.
bool rsaVerifySha256(const RsaPublicKey& key, const uint8_t sig[kRsaBytes], const uint8_t hash[32]);
