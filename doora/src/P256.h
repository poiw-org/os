// ECDSA P-256 / SHA-256 verification, all the door needs to check that a phone holds the private
// key its credential names. Wraps micro-ecc (vendor/micro-ecc); portable and allocation-free.
#pragma once
#include <stdint.h>

// `pub` is the uncompressed point 04 || X || Y, `hash` the SHA-256 of the signed message and `sig`
// the raw r || s (32 bytes each, big-endian). False for a point that is not on the curve, for r or
// s of zero or not below the group order, and for a signature that does not match.
// High-s signatures are accepted: a nonce is used once, so malleability gains an attacker nothing.
bool p256Verify(const uint8_t pub[65], const uint8_t hash[32], const uint8_t sig[64]);

// True for 04 || X || Y with X and Y on the curve.
bool p256ValidPoint(const uint8_t pub[65]);
