// The door's idea of "now" in epoch seconds, built to survive power cuts and missing networks.
//
// - Synced: SNTP or the Supabase server time was received since boot. Exact.
// - Estimated: no sync yet. The time is the newest of (last checkpoint in flash, firmware build
//   time) plus uptime. It is a lower bound: a long blackout makes it lag, never lead.
// A checkpoint is written to flash every 10 minutes, so a blackout costs at most that much.
#pragma once
#include <stdint.h>

class Clock {
 public:
	void begin(const char* ntpServer);
	uint32_t now() const;
	bool synced() const;
	// Used when NTP is blocked but the Supabase API is reachable.
	void adoptServerTime(uint32_t epoch);
	void service();

 private:
	uint32_t baseEpoch_ = 0;
	uint32_t baseMs_ = 0;
	uint32_t lastSavedEpoch_ = 0;
	uint32_t lastSaveMs_ = 0;
	bool savedSinceSync_ = false;
};
