#include "Rsa.h"
#include <string.h>
#include "Sha256.h"

namespace {
constexpr int kLimbs = kRsaBytes / 4;

void load(const uint8_t* be, uint32_t* x) {
	for (int i = 0; i < kLimbs; i++) {
		const uint8_t* p = be + kRsaBytes - 4 * (i + 1);
		x[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
	}
}

void store(const uint32_t* x, uint8_t* be) {
	for (int i = 0; i < kLimbs; i++) {
		uint8_t* p = be + kRsaBytes - 4 * (i + 1);
		p[0] = (uint8_t)(x[i] >> 24);
		p[1] = (uint8_t)(x[i] >> 16);
		p[2] = (uint8_t)(x[i] >> 8);
		p[3] = (uint8_t)x[i];
	}
}

bool geq(const uint32_t* a, const uint32_t* b) {
	for (int i = kLimbs - 1; i >= 0; i--)
		if (a[i] != b[i]) return a[i] > b[i];
	return true;
}

void sub(uint32_t* a, const uint32_t* b) {
	int64_t borrow = 0;
	for (int i = 0; i < kLimbs; i++) {
		int64_t d = (int64_t)a[i] - b[i] - borrow;
		borrow = d < 0;
		a[i] = (uint32_t)d;
	}
}

// Montgomery product a*b/2^2048 mod n (CIOS). Inputs are below n.
void montMul(const uint32_t* a, const uint32_t* b, const uint32_t* n, uint32_t n0inv, uint32_t* out) {
	uint32_t t[kLimbs + 2] = {0};
	for (int i = 0; i < kLimbs; i++) {
		uint64_t carry = 0;
		for (int j = 0; j < kLimbs; j++) {
			uint64_t s = (uint64_t)t[j] + (uint64_t)a[j] * b[i] + carry;
			t[j] = (uint32_t)s;
			carry = s >> 32;
		}
		uint64_t s = (uint64_t)t[kLimbs] + carry;
		t[kLimbs] = (uint32_t)s;
		t[kLimbs + 1] = (uint32_t)(s >> 32);

		uint32_t m = t[0] * n0inv;
		carry = ((uint64_t)t[0] + (uint64_t)m * n[0]) >> 32;
		for (int j = 1; j < kLimbs; j++) {
			s = (uint64_t)t[j] + (uint64_t)m * n[j] + carry;
			t[j - 1] = (uint32_t)s;
			carry = s >> 32;
		}
		s = (uint64_t)t[kLimbs] + carry;
		t[kLimbs - 1] = (uint32_t)s;
		t[kLimbs] = t[kLimbs + 1] + (uint32_t)(s >> 32);
	}
	memcpy(out, t, sizeof(uint32_t) * kLimbs);
	if (t[kLimbs] || geq(out, n)) sub(out, n);
}

// DigestInfo prefix for SHA-256 (RFC 8017 section 9.2, note 1).
const uint8_t kDigestInfo[19] = {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
                                 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
}  // namespace

bool rsaVerifySha256(const uint8_t nBytes[kRsaBytes], const uint8_t sigBytes[kRsaBytes], const uint8_t hash[32]) {
	if (!(nBytes[0] & 0x80) || !(nBytes[kRsaBytes - 1] & 1)) return false;
	uint32_t n[kLimbs], s[kLimbs];
	load(nBytes, n);
	load(sigBytes, s);
	if (geq(s, n)) return false;

	// -n^-1 mod 2^32 by Newton iteration (n is odd).
	uint32_t inv = n[0];
	for (int i = 0; i < 5; i++) inv *= 2 - n[0] * inv;
	uint32_t n0inv = 0u - inv;

	// R^2 mod n, where R = 2^2048: start from 1 and double 4096 times.
	uint32_t r2[kLimbs] = {1};
	for (int i = 0; i < 2 * kLimbs * 32; i++) {
		uint32_t carry = 0;
		for (int j = 0; j < kLimbs; j++) {
			uint32_t next = r2[j] >> 31;
			r2[j] = r2[j] << 1 | carry;
			carry = next;
		}
		if (carry || geq(r2, n)) sub(r2, n);
	}

	// s^65537 = (s^(2^16)) * s, in Montgomery form.
	uint32_t sm[kLimbs], acc[kLimbs], one[kLimbs] = {1};
	montMul(s, r2, n, n0inv, sm);
	memcpy(acc, sm, sizeof(acc));
	for (int i = 0; i < 16; i++) montMul(acc, acc, n, n0inv, acc);
	montMul(acc, sm, n, n0inv, acc);
	montMul(acc, one, n, n0inv, acc);

	uint8_t em[kRsaBytes], expected[kRsaBytes];
	store(acc, em);
	memset(expected, 0xff, sizeof(expected));
	expected[0] = 0x00;
	expected[1] = 0x01;
	size_t tail = sizeof(kDigestInfo) + 32;
	expected[kRsaBytes - tail - 1] = 0x00;
	memcpy(expected + kRsaBytes - tail, kDigestInfo, sizeof(kDigestInfo));
	memcpy(expected + kRsaBytes - 32, hash, 32);
	return constantTimeEqual(em, expected, kRsaBytes);
}
