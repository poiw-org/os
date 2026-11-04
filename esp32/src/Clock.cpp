#include "Clock.h"
#include <Arduino.h>
#include <atomic>
#include <mutex>
#include <esp_sntp.h>
#include <time.h>
#include <sys/time.h>
#include "Log.h"
#include "Persist.h"
#include "TimeUtil.h"

namespace {
constexpr uint32_t kCheckpointMs = 600000;
// The server time wins when the clock is further off than this; SNTP is refused beyond it.
constexpr int32_t kMaxSkewS = 120;
// SNTP may land this far below the floor (a checkpoint is rounded to when it was written).
constexpr uint32_t kFloorMarginS = 60;

// Shared with the SNTP client (lwIP task) and the REST task, which delivers the server time.
std::mutex gLock;
std::atomic<bool> gSynced{false};
std::atomic<bool> gServerSynced{false};
std::atomic<uint32_t> gFloor{0};        // the clock never goes below this: build, checkpoint, server time
std::atomic<uint32_t> gServerEpoch{0};  // last server time and the millis() it arrived at
std::atomic<uint32_t> gServerMs{0};

uint32_t serverAnchored() { return gServerEpoch.load() + (millis() - gServerMs.load()) / 1000; }

bool sntpAcceptable(uint32_t t) {
	if (gServerSynced.load()) {
		int32_t skew = (int32_t)(t - serverAnchored());
		return skew <= kMaxSkewS && skew >= -kMaxSkewS;
	}
	return t + kFloorMarginS >= gFloor.load();
}
}  // namespace

// Replaces ESP-IDF's weak default, which sets the system time to whatever the SNTP reply says.
// The reply is checked first, so a forged one never touches the clock (TLS reads it too).
extern "C" void sntp_sync_time(struct timeval* tv) {
	if (!tv) return;
	uint32_t t = (uint32_t)tv->tv_sec;
	{
		std::lock_guard<std::mutex> hold(gLock);
		if (!sntpAcceptable(t)) {
			LOG("Clock: ignored an NTP time (%lu) that disagrees with the %s.", (unsigned long)t,
			    gServerSynced.load() ? "server time" : "stored checkpoint");
			return;
		}
		settimeofday(tv, nullptr);
		gSynced = true;
	}
	sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}

void Clock::begin(const char* ntpServer) {
	uint32_t saved = Persist::loadClock();
	uint32_t built = buildEpoch(__DATE__, __TIME__);
	baseEpoch_ = saved > built ? saved : built;
	baseMs_ = millis();
	lastSavedEpoch_ = saved;
	lastSaveMs_ = baseMs_;
	gFloor = baseEpoch_;
	LOG("Clock: estimated start %lu (checkpoint %lu, build %lu)", (unsigned long)baseEpoch_, (unsigned long)saved,
	    (unsigned long)built);

	// TLS checks certificate validity against the system time, so give it the estimate until a
	// real sync arrives (certificates last months; a lagging estimate is fine).
	timeval estimate = {(time_t)baseEpoch_, 0};
	settimeofday(&estimate, nullptr);
	configTime(0, 0, ntpServer);
}

bool Clock::synced() const { return gSynced.load(); }

bool Clock::reliable() const { return !gSynced.load() || gServerSynced.load(); }

uint32_t Clock::estimate() const { return baseEpoch_ + (millis() - baseMs_) / 1000; }

uint32_t Clock::now() const {
	if (!gSynced.load()) return estimate();
	uint32_t t = (uint32_t)time(nullptr);
	uint32_t floor = gFloor.load();
	return t > floor ? t : floor;
}

void Clock::adoptServerTime(uint32_t epoch) {
	if (!epoch) return;
	std::lock_guard<std::mutex> hold(gLock);
	uint32_t ms = millis();
	bool wasSynced = gSynced.load();
	int32_t skew = (int32_t)((uint32_t)time(nullptr) - epoch);
	gServerEpoch = epoch;
	gServerMs = ms;
	gFloor = epoch;  // the server is the reference, even below a checkpoint that was pushed ahead
	gServerSynced = true;
	if (wasSynced && skew <= kMaxSkewS && skew >= -kMaxSkewS) return;
	timeval tv = {(time_t)epoch, 0};
	settimeofday(&tv, nullptr);
	gSynced = true;
	if (wasSynced) LOG("Clock: was %ld s off the Supabase server time; corrected.", (long)skew);
	else LOG("Clock: set from the Supabase server time.");
}

void Clock::service() {
	uint32_t ms = millis();
	// Fold elapsed time into the base so the estimate survives millis() wrapping (every 49 days).
	uint32_t whole = (ms - baseMs_) / 1000;
	baseEpoch_ += whole;
	baseMs_ += whole * 1000;

	bool server = gServerSynced.load();
	if (!server) {
		// The lag-only estimate is a lower bound on the real time, so the floor follows it: a forged
		// SNTP reply after a long offline uptime can then not roll the clock back towards boot time.
		std::lock_guard<std::mutex> hold(gLock);
		uint32_t e = estimate();
		if (!gServerSynced.load() && e > gFloor.load()) gFloor = e;
	}
	bool due = ms - lastSaveMs_ >= kCheckpointMs || (server && !savedSinceServer_);
	if (!due) return;
	// Never checkpoint a time SNTP alone vouches for: the lag-only estimate stands in for it.
	uint32_t t = server ? now() : estimate();
	lastSaveMs_ = ms;
	// The checkpoint only moves forward, except that the server time may pull one that is ahead of
	// it (written from a forged time by older firmware) back down.
	bool repair = server && t + (uint32_t)kMaxSkewS < lastSavedEpoch_;
	if (!repair && t <= lastSavedEpoch_) return;
	if (Persist::saveClock(t)) {
		if (repair) LOG("Clock: stored checkpoint %lu was ahead of the server time; rewritten.", (unsigned long)lastSavedEpoch_);
		lastSavedEpoch_ = t;
		if (server) savedSinceServer_ = true;
	}
}
