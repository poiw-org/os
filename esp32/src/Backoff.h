// Retry delays that double up to a cap, with jitter so several doors recovering from the same
// outage do not reach the server in step. Pure arithmetic on millis() values, so it is unit tested.
#pragma once
#include <stdint.h>

class Backoff {
 public:
	constexpr Backoff(uint32_t firstMs, uint32_t maxMs) : first_(firstMs), max_(maxMs) {}

	bool due(uint32_t now) const { return !armed_ || (int32_t)(now - next_) >= 0; }
	// An attempt worked: the next one may start at once and the delays start over.
	void succeed() { fails_ = 0; armed_ = false; }
	// An attempt failed. `random` supplies the jitter (up to a quarter of the delay). Returns the
	// delay chosen.
	uint32_t fail(uint32_t now, uint32_t random) {
		uint32_t wait = first_;
		for (uint8_t i = 0; i < fails_ && wait < max_; i++) wait *= 2;
		if (wait > max_) wait = max_;
		wait += random % (wait / 4 + 1);
		if (fails_ < 31) fails_++;
		hold(now, wait);
		return wait;
	}
	// Waits a fixed time, for a refusal that retrying sooner cannot fix.
	void hold(uint32_t now, uint32_t ms) {
		next_ = now + ms;
		armed_ = true;
	}
	uint8_t failures() const { return fails_; }

 private:
	uint32_t first_, max_;
	uint32_t next_ = 0;
	uint8_t fails_ = 0;
	bool armed_ = false;
};
