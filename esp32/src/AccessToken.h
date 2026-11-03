// The door's Supabase access token. The REST task signs in and stores it; the realtime task
// copies it into its join and renewal messages. A mutex guards the text, and the generation counter
// lets a reader notice a new token without taking it.
#pragma once
#include <atomic>
#include <mutex>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

class AccessToken {
 public:
	static constexpr size_t kMax = 1400;

	bool present() const { return present_.load(std::memory_order_acquire); }
	// Changes whenever the token is replaced or cleared.
	uint32_t generation() const { return generation_.load(std::memory_order_acquire); }

	bool set(const char* token) {
		size_t n = strlen(token);
		if (!n || n >= kMax) return false;
		std::lock_guard<std::mutex> hold(lock_);
		memcpy(text_, token, n + 1);
		present_.store(true, std::memory_order_release);
		generation_.fetch_add(1, std::memory_order_acq_rel);
		return true;
	}

	void clear() {
		std::lock_guard<std::mutex> hold(lock_);
		text_[0] = 0;
		present_.store(false, std::memory_order_release);
		generation_.fetch_add(1, std::memory_order_acq_rel);
	}

	// Copies the token (NUL terminated) to `out`, which holds `cap` bytes. False when there is no
	// token or it does not fit. `generation` receives the generation of the copied token.
	bool copyTo(char* out, size_t cap, uint32_t* generation = nullptr) const {
		std::lock_guard<std::mutex> hold(lock_);
		size_t n = strlen(text_);
		if (!n || n >= cap) return false;
		memcpy(out, text_, n + 1);
		if (generation) *generation = generation_.load(std::memory_order_acquire);
		return true;
	}

 private:
	mutable std::mutex lock_;
	char text_[kMax] = {0};
	std::atomic<bool> present_{false};
	std::atomic<uint32_t> generation_{0};
};
