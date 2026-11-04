// The door's idea of "now" in epoch seconds, built to survive power cuts, missing networks and a
// network that lies about the time.
//
// - Estimated: no sync yet. The time is the newest of (last checkpoint in flash, firmware build
//   time) plus uptime. It is a lower bound: a long blackout makes it lag, never lead.
// - Synced: SNTP or the Supabase server time was received since boot.
//
// SNTP is plain UDP, so anyone on the network path can answer it. The Supabase server time comes
// over TLS and is the reference: it sets the clock whenever SNTP is missing or disagrees with it.
// SNTP alone may never move the clock below the checkpoint or build time (that would revive
// expired keys), and once the server time is known SNTP may only nudge the clock by a little.
// Only a time that is a lower bound or that the server confirmed is written to flash or used to
// forget blacklist entries.
// A checkpoint is written to flash every 10 minutes, so a blackout costs at most that much.
#pragma once
#include <stdint.h>

class Clock {
 public:
	void begin(const char* ntpServer);
	uint32_t now() const;
	bool synced() const;
	// True when now() is safe to forget expired blacklist entries by: it is either the lag-only
	// estimate or the server confirmed it. False while the time comes from SNTP alone.
	bool reliable() const;
	// The Supabase server time (authenticated by TLS). Applied whenever the clock is unsynced or off
	// by more than a couple of minutes, and it may move a checkpoint that was pushed ahead back down.
	void adoptServerTime(uint32_t epoch);
	void service();

 private:
	uint32_t estimate() const;

	uint32_t baseEpoch_ = 0;
	uint32_t baseMs_ = 0;
	uint32_t lastSavedEpoch_ = 0;
	uint32_t lastSaveMs_ = 0;
	bool savedSinceServer_ = false;
};
