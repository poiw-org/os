// Offline verification of the key tokens written to NFC tags. v2 tokens are RS256, signed by the
// `keys` edge function and checked here with the public key; v1 tokens are HS256 with a shared secret.
// Claims: sub = user id, jti = key id, aud = tag serial number or "ble", iat/nbf/exp = seconds.
// A phone credential (aud "ble") also carries cnf, the phone's P-256 public key.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Rsa.h"

// Who a token is for. A tag token names a serial number; a BLE credential is for a phone and is
// never valid on a tag, and a tag token is never valid over BLE (see Access.h).
enum class Audience : uint8_t { Tag, Ble };

constexpr size_t kCnfLen = 65;  // uncompressed P-256 point: 04 || X || Y

struct KeyClaims {
	char sub[40];
	char jti[40];
	uint8_t aud[10];
	uint8_t audLen;       // 0 for a BLE credential
	Audience audience;
	uint8_t cnf[kCnfLen];  // only meaningful when audience is Ble; the format is checked here, the
	                       // point on the curve by p256Verify
	uint32_t iat, nbf, exp;
};

enum class JwtResult : uint8_t { Ok, Malformed, BadSignature, BadClaims, NoPublicKey };

// What a token can be checked against. Each algorithm only ever uses its own material, so a token
// cannot pick the way it is verified beyond the two algorithms the door knows.
struct VerifyKeys {
	const uint8_t* hmacSecret;  // v1 (HS256)
	size_t hmacSecretLen;
	const RsaPublicKey* rsaKey;  // v2 (RS256), or null while unknown
};

constexpr size_t kMaxTokenLen = 860;  // fills an NTAG216

// Verifies the signature first and only then reads the claims.
JwtResult jwtVerify(const char* token, size_t len, const VerifyKeys& keys, KeyClaims& out);

// Reads the claims WITHOUT checking the signature. Only for identifying a tag whose token did not
// verify; never for an access decision.
JwtResult jwtPeek(const char* token, size_t len, KeyClaims& out);

// Exposed for tests: base64url without padding. Returns the decoded size or -1.
int base64UrlDecode(const char* in, size_t len, uint8_t* out, size_t cap);
