#include "Jwt.h"
#include <stdlib.h>
#include <string.h>
#include "Rsa.h"
#include "Sha256.h"
#include "TimeUtil.h"

int base64UrlDecode(const char* in, size_t len, uint8_t* out, size_t cap) {
	uint32_t acc = 0;
	int bits = 0;
	size_t n = 0;
	for (size_t i = 0; i < len; i++) {
		char c = in[i];
		int v = (c >= 'A' && c <= 'Z') ? c - 'A' : (c >= 'a' && c <= 'z') ? c - 'a' + 26
		        : (c >= '0' && c <= '9') ? c - '0' + 52 : c == '-' ? 62 : c == '_' ? 63 : -1;
		if (v < 0) return -1;
		acc = (acc << 6) | (uint32_t)v;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			if (n >= cap) return -1;
			out[n++] = (uint8_t)(acc >> bits);
		}
	}
	return (int)n;
}

// Finds `"key":` in a flat JSON object and returns a pointer at its value. The text has already
// been authenticated, so a tolerant scan is enough; it never reads past the terminator.
static const char* findValue(const char* json, const char* key) {
	char pattern[16];
	size_t k = strlen(key);
	if (k + 3 > sizeof(pattern)) return nullptr;
	pattern[0] = '"';
	memcpy(pattern + 1, key, k);
	pattern[k + 1] = '"';
	pattern[k + 2] = 0;
	for (const char* p = json; (p = strstr(p, pattern)); p += k + 2) {
		const char* v = p + k + 2;
		while (*v == ' ') v++;
		if (*v != ':') continue;
		v++;
		while (*v == ' ') v++;
		return v;
	}
	return nullptr;
}

static bool readString(const char* json, const char* key, char* out, size_t cap) {
	const char* v = findValue(json, key);
	if (!v || *v != '"') return false;
	v++;
	size_t n = 0;
	while (*v && *v != '"') {
		if (*v == '\\' || n + 1 >= cap) return false;
		out[n++] = *v++;
	}
	if (*v != '"') return false;
	out[n] = 0;
	return true;
}

static bool readUint(const char* json, const char* key, uint32_t& out) {
	const char* v = findValue(json, key);
	if (!v || *v < '0' || *v > '9') return false;
	char* end;
	unsigned long long n = strtoull(v, &end, 10);
	if (n > 0xFFFFFFFFull) return false;
	out = (uint32_t)n;
	return true;
}

// Reads the claims from the base64url payload.
static JwtResult readClaims(const char* encoded, size_t len, KeyClaims& out) {
	char payload[576];
	int p = base64UrlDecode(encoded, len, (uint8_t*)payload, sizeof(payload) - 1);
	if (p < 0) return JwtResult::Malformed;
	payload[p] = 0;

	// Legacy (pre-Supabase) tokens may write aud as "0x04 0xa1 ..." and leave out jti or nbf, which
	// the old firmware accepted, so only aud and exp are required.
	char aud[64];
	memset(&out, 0, sizeof(out));
	if (!readString(payload, "aud", aud, sizeof(aud)) || !readUint(payload, "exp", out.exp))
		return JwtResult::BadClaims;
	readString(payload, "jti", out.jti, sizeof(out.jti));
	readUint(payload, "nbf", out.nbf);
	readString(payload, "sub", out.sub, sizeof(out.sub));
	readUint(payload, "iat", out.iat);

	int uid = parseUid(aud, out.aud, sizeof(out.aud));
	if (uid <= 0) return JwtResult::BadClaims;
	out.audLen = (uint8_t)uid;
	return JwtResult::Ok;
}

JwtResult jwtVerify(const char* token, size_t len, const VerifyKeys& keys, KeyClaims& out) {
	if (len < 16 || len > kMaxTokenLen) return JwtResult::Malformed;
	const char* dot1 = (const char*)memchr(token, '.', len);
	if (!dot1) return JwtResult::Malformed;
	const char* dot2 = (const char*)memchr(dot1 + 1, '.', len - (size_t)(dot1 + 1 - token));
	if (!dot2) return JwtResult::Malformed;

	size_t headerLen = (size_t)(dot1 - token);
	size_t payloadLen = (size_t)(dot2 - dot1 - 1);
	size_t sigLen = len - (size_t)(dot2 - token) - 1;
	if (headerLen > 64 || payloadLen > 760) return JwtResult::Malformed;

	char header[48];
	int h = base64UrlDecode(token, headerLen, (uint8_t*)header, sizeof(header) - 1);
	if (h < 0) return JwtResult::Malformed;
	header[h] = 0;

	// The signature is checked before anything else is read. The algorithm named in the header
	// only selects between the two schemes; each has its own key.
	const size_t signedLen = (size_t)(dot2 - token);
	if (strstr(header, "RS256")) {
		uint8_t sig[kRsaBytes + 1];
		if (sigLen != 342 || base64UrlDecode(dot2 + 1, sigLen, sig, sizeof(sig)) != (int)kRsaBytes)
			return JwtResult::Malformed;
		if (!keys.rsaKey) return JwtResult::NoPublicKey;
		uint8_t digest[32];
		Sha256 sha;
		sha.update((const uint8_t*)token, signedLen);
		sha.final(digest);
		if (!rsaVerifySha256(*keys.rsaKey, sig, digest)) return JwtResult::BadSignature;
	} else if (strstr(header, "HS256")) {
		uint8_t sig[33];
		if (sigLen != 43 || base64UrlDecode(dot2 + 1, sigLen, sig, sizeof(sig)) != 32) return JwtResult::Malformed;
		// No v1 secret configured: v1 keys are off. An empty HMAC key is public knowledge.
		if (!keys.hmacSecret || !keys.hmacSecretLen) return JwtResult::BadSignature;
		uint8_t expected[32];
		hmacSha256(keys.hmacSecret, keys.hmacSecretLen, (const uint8_t*)token, signedLen, expected);
		if (!constantTimeEqual(sig, expected, 32)) return JwtResult::BadSignature;
	} else {
		return JwtResult::BadClaims;
	}

	return readClaims(dot1 + 1, payloadLen, out);
}

JwtResult jwtPeek(const char* token, size_t len, KeyClaims& out) {
	memset(&out, 0, sizeof(out));
	if (len < 16 || len > kMaxTokenLen) return JwtResult::Malformed;
	const char* dot1 = (const char*)memchr(token, '.', len);
	if (!dot1) return JwtResult::Malformed;
	const char* dot2 = (const char*)memchr(dot1 + 1, '.', len - (size_t)(dot1 + 1 - token));
	if (!dot2 || dot2 - dot1 - 1 > 760) return JwtResult::Malformed;
	return readClaims(dot1 + 1, (size_t)(dot2 - dot1 - 1), out);
}
