#include "Clock.h"
#include <Arduino.h>
#include <esp_sntp.h>
#include <time.h>
#include <sys/time.h>
#include "Log.h"
#include "Persist.h"
#include "TimeUtil.h"

namespace {
volatile bool gSynced = false;
constexpr uint32_t kCheckpointMs = 600000;

// Called by the SNTP client when it has set the system time.
void onTimeSet(timeval*) { gSynced = true; }
}  // namespace

void Clock::begin(const char* ntpServer) {
	uint32_t saved = Persist::loadClock();
	uint32_t built = buildEpoch(__DATE__, __TIME__);
	baseEpoch_ = saved > built ? saved : built;
	baseMs_ = millis();
	lastSavedEpoch_ = saved;
	lastSaveMs_ = baseMs_;
	LOG("Clock: estimated start %lu (checkpoint %lu, build %lu)", (unsigned long)baseEpoch_, (unsigned long)saved,
	    (unsigned long)built);

	// TLS checks certificate validity against the system time, so give it the estimate until a
	// real sync arrives (certificates last months; a lagging estimate is fine).
	timeval estimate = {(time_t)baseEpoch_, 0};
	settimeofday(&estimate, nullptr);
	sntp_set_time_sync_notification_cb(onTimeSet);
	configTime(0, 0, ntpServer);
}

bool Clock::synced() const { return gSynced; }

uint32_t Clock::now() const {
	if (gSynced) return (uint32_t)time(nullptr);
	return baseEpoch_ + (millis() - baseMs_) / 1000;
}

void Clock::adoptServerTime(uint32_t epoch) {
	if (gSynced || !epoch) return;
	timeval tv = {(time_t)epoch, 0};
	settimeofday(&tv, nullptr);
	gSynced = true;
	LOG("Clock: set from the Supabase server time.");
}

void Clock::service() {
	uint32_t ms = millis();
	// Fold elapsed time into the base so the estimate survives millis() wrapping (every 49 days).
	uint32_t whole = (ms - baseMs_) / 1000;
	baseEpoch_ += whole;
	baseMs_ += whole * 1000;

	bool due = ms - lastSaveMs_ >= kCheckpointMs || (gSynced && !savedSinceSync_);
	if (!due) return;
	uint32_t t = now();
	lastSaveMs_ = ms;
	if (t <= lastSavedEpoch_) return;  // the checkpoint only ever moves forward
	if (Persist::saveClock(t)) {
		lastSavedEpoch_ = t;
		if (gSynced) savedSinceSync_ = true;
	}
}
