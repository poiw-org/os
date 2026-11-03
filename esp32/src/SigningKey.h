// The public half of the key pair that signs v2 key tokens. The REST task stores it when it learns
// it from the backoffice; the main loop reads it for every tag. A mutex guards the bytes.
#pragma once
#include <atomic>
#include <mutex>
#include <string.h>
#include "Rsa.h"

class SigningKey {
 public:
	bool present() const { return present_.load(std::memory_order_acquire); }

	// False when `n` cannot be a 2048-bit RSA modulus.
	bool set(const uint8_t n[kRsaBytes]) {
		if (!(n[0] & 0x80) || !(n[kRsaBytes - 1] & 1)) return false;
		std::lock_guard<std::mutex> hold(lock_);
		memcpy(n_, n, kRsaBytes);
		present_.store(true, std::memory_order_release);
		return true;
	}

	// Copies the modulus out; false when none is known yet.
	bool copyTo(uint8_t out[kRsaBytes]) const {
		if (!present()) return false;
		std::lock_guard<std::mutex> hold(lock_);
		memcpy(out, n_, kRsaBytes);
		return true;
	}

	bool sameAs(const uint8_t n[kRsaBytes]) const {
		std::lock_guard<std::mutex> hold(lock_);
		return present() && memcmp(n_, n, kRsaBytes) == 0;
	}

 private:
	mutable std::mutex lock_;
	uint8_t n_[kRsaBytes] = {0};
	std::atomic<bool> present_{false};
};
