// Cap on restarts the door triggers itself to recover from a fault (e.g. the PN532 not answering):
// at most kMaxSelfResets in any rolling 24 hours, so a fault a restart cannot fix never becomes a
// reboot loop. Pure arithmetic on epoch seconds, so it is unit tested on the host.
#pragma once
#include <stdint.h>

constexpr uint8_t kMaxSelfResets = 3;
constexpr uint32_t kResetWindowS = 24UL * 3600;

struct ResetLog {
	uint32_t at[kMaxSelfResets] = {0, 0, 0};  // epoch seconds of the latest self-restarts, 0 = unused
};

// True when `t` is a restart that still counts against the cap. One stamped further in the future
// than the window (a clock that was wrong when it was written) is ignored rather than blocking
// restarts for good.
inline bool resetCounts(uint32_t t, uint32_t now) {
	if (t == 0) return false;
	int64_t age = (int64_t)now - (int64_t)t;
	return age < (int64_t)kResetWindowS && age > -(int64_t)kResetWindowS;
}

inline bool resetAllowed(const ResetLog& log, uint32_t now) {
	uint8_t used = 0;
	for (uint32_t t : log.at) used += resetCounts(t, now);
	return used < kMaxSelfResets;
}

// Records a restart at `now` in the slot that counts least: an unused or expired one, else the oldest.
inline void resetRecord(ResetLog& log, uint32_t now) {
	uint8_t pick = 0;
	for (uint8_t i = 0; i < kMaxSelfResets; i++) {
		if (!resetCounts(log.at[i], now)) { pick = i; break; }
		if (log.at[i] < log.at[pick]) pick = i;
	}
	log.at[pick] = now;
}
