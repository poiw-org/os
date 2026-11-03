// SHA-256 and HMAC-SHA256, small and allocation-free. Verified against the RFC 4231 vectors in
// test/test.cpp. Kept portable so the whole access decision can be tested on a workstation.
#pragma once
#include <stddef.h>
#include <stdint.h>

class Sha256 {
 public:
	Sha256() { reset(); }
	void reset();
	void update(const uint8_t* data, size_t len);
	void final(uint8_t out[32]);

 private:
	void block(const uint8_t* p);
	uint32_t h_[8];
	uint8_t buf_[64];
	uint64_t total_;
	size_t fill_;
};

void hmacSha256(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen,
                uint8_t out[32]);

// Compares without an early exit so the signature check does not leak timing.
bool constantTimeEqual(const uint8_t* a, const uint8_t* b, size_t len);
