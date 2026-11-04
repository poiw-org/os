// The public half of the key pair that signs v2 key tokens. The REST task stores it when it learns
// it from the backoffice; the main loop reads it for every tag. It is prepared for verification
// here, on the task that sets it, so a tap never pays for that. A mutex guards the key.
#pragma once
#include <atomic>
#include <mutex>
#include <string.h>
#include "Rsa.h"

class SigningKey {
 public:
	bool present() const { return present_.load(std::memory_order_acquire); }

	// False when `n` cannot be a 2048-bit RSA modulus. Only called when the key changes, so holding
	// the lock while preparing (milliseconds) is fine; rsaPrepare() checks before it writes anything.
	bool set(const uint8_t n[kRsaBytes]) {
		std::lock_guard<std::mutex> hold(lock_);
		if (!rsaPrepare(n, key_)) return false;
		memcpy(n_, n, kRsaBytes);
		present_.store(true, std::memory_order_release);
		return true;
	}

	// Copies the prepared key out; false when none is known yet.
	bool copyTo(RsaPublicKey& out) const {
		if (!present()) return false;
		std::lock_guard<std::mutex> hold(lock_);
		out = key_;
		return true;
	}

	bool sameAs(const uint8_t n[kRsaBytes]) const {
		std::lock_guard<std::mutex> hold(lock_);
		return present() && memcmp(n_, n, kRsaBytes) == 0;
	}

 private:
	mutable std::mutex lock_;
	uint8_t n_[kRsaBytes] = {0};
	RsaPublicKey key_ = {};
	std::atomic<bool> present_{false};
};
