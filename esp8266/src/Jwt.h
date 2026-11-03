// Offline verification of the HS256 key tokens written to NFC tags by the `keys` edge function.
// Claims: sub = user id, jti = key id, aud = tag serial number, iat/nbf/exp = seconds.
#pragma once
#include <stddef.h>
#include <stdint.h>

struct KeyClaims {
	char sub[40];
	char jti[40];
	uint8_t aud[10];
	uint8_t audLen;
	uint32_t iat, nbf, exp;
};

enum class JwtResult : uint8_t { Ok, Malformed, BadSignature, BadClaims };

constexpr size_t kMaxTokenLen = 640;

// Verifies the signature first and only then reads the claims.
JwtResult jwtVerify(const char* token, size_t len, const uint8_t* secret, size_t secretLen,
                    KeyClaims& out);

// Exposed for tests: base64url without padding. Returns the decoded size or -1.
int base64UrlDecode(const char* in, size_t len, uint8_t* out, size_t cap);
