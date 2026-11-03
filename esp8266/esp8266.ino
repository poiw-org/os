// Doora | po/iw hackerspace | https://github.com/poiw-org/doora
//
// Door controller. Taps are decided locally from the signed token on the tag, the clock and a
// persisted blacklist, so the door keeps working through Wi-Fi, internet and power outages.
// Supabase is used to keep that local state fresh and to report what happened. See README.md.
#include <Arduino.h>
#include "src/Access.h"
#include "src/Actuators.h"
#include "src/BlacklistSet.h"
#include "src/Clock.h"
#include "src/Cloud.h"
#include "src/Console.h"
#include "src/Journal.h"
#include "src/Jwt.h"
#include "src/Log.h"
#include "src/Persist.h"
#include "src/Reader.h"
#include "src/Settings.h"

namespace {
Settings settings;
Clock clock_;
BlacklistSet blacklist;
Actuators actuators;
Reader reader;
Cloud cloud;
TagRead tag;

uint32_t lastTapMs = 0;
uint32_t lastFailureLogMs = 0;
uint32_t lastHousekeepingMs = 0;
uint8_t lowHeapStrikes = 0;

constexpr uint32_t kQuietAfterTapMs = 4000;     // network maintenance waits this long after a tap
constexpr uint32_t kFailureLogGapMs = 3000;     // rate limit for logging denied taps
constexpr uint32_t kMaxUptimeMs = 7UL * 24 * 3600 * 1000;
constexpr uint32_t kLowHeapBytes = 7000;

// A new journal record for "now".
JournalRecord newRecord() {
	JournalRecord r;
	memset(&r, 0, sizeof(r));
	r.ts = clock_.now();
	if (!clock_.synced()) r.flags |= kClockEstimated;
	if (!cloud.online()) r.flags |= kOffline;
	return r;
}

void record(const JournalRecord& r) {
	if (!Persist::journalAppend(r)) LOG("Could not store the event; it is lost.");
}

void onForceOpen() {
	actuators.grant();
	lastTapMs = millis();
	JournalRecord r = newRecord();
	r.allowed = 1;
	r.flags |= kForced;
	record(r);
	LOG("Door opened (force open).");
}

void handleTag() {
	lastTapMs = millis();
	JournalRecord r = newRecord();
	r.uidLen = tag.uidLen;
	memcpy(r.uid, tag.uid, tag.uidLen);

	KeyClaims key;
	JwtResult parsed = tag.hasToken ? jwtVerify(tag.token, tag.tokenLen, (const uint8_t*)settings.jwtSecret,
	                                            strlen(settings.jwtSecret), key)
	                                : JwtResult::Malformed;
	if (parsed != JwtResult::Ok) {
		actuators.deny();
		r.verdict = 255;  // no Verdict applies; stored as reason "Unknown"
		r.flags |= kInvalidToken;
		LOG("Denied: the tag carries no valid key (%s).", tag.hasToken ? "bad signature or claims" : "no token");
		if (millis() - lastFailureLogMs >= kFailureLogGapMs || !lastFailureLogMs) {
			lastFailureLogMs = millis();
			record(r);
		}
		return;
	}

	r.keyLen = (uint8_t)strnlen(key.jti, sizeof(r.key));
	memcpy(r.key, key.jti, r.keyLen);

	Verdict v = decide(key, tag.uid, tag.uidLen, r.ts, clock_.synced(), blacklist.contains(key.jti));
	if (v == Verdict::Allow) {
		actuators.grant();
		r.allowed = 1;
		LOG("Door opened for key %s.", key.jti);
		record(r);
	} else {
		actuators.deny();
		r.verdict = (uint8_t)v;
		LOG("Denied key %s: %s.", key.jti, reasonName(v));
		if (millis() - lastFailureLogMs >= kFailureLogGapMs || !lastFailureLogMs) {
			lastFailureLogMs = millis();
			record(r);
		}
	}
}

void printStatus() {
	LOG("--- status ---");
	LOG("uptime    : %lu s, free heap %u, largest block %u", (unsigned long)(millis() / 1000), ESP.getFreeHeap(),
	    ESP.getMaxFreeBlockSize());
	LOG("clock     : %lu (%s)", (unsigned long)clock_.now(), clock_.synced() ? "synced" : "estimated");
	LOG("blacklist : %u disabled keys", (unsigned)blacklist.size());
	LOG("journal   : %u events waiting, %lu dropped", (unsigned)Persist::journalCount(),
	    (unsigned long)Persist::journalDropped());
	cloud.printStatus();
}

// Periodic upkeep that never matters to a tap.
void housekeeping(uint32_t ms) {
	if (ms - lastHousekeepingMs < 60000) return;
	lastHousekeepingMs = ms;

	blacklist.prune(clock_.now());

	bool idle = !actuators.busy() && ms - lastTapMs > 60000;
	if (!reader.healthy() && idle) {
		LOG("The NFC reader stopped answering; restarting.");
		delay(100);
		ESP.restart();
	}
	// A leak or fragmentation would eventually stop TLS from working. Everything that matters is
	// persisted, so a restart in an idle moment costs nothing.
	lowHeapStrikes = ESP.getFreeHeap() < kLowHeapBytes ? lowHeapStrikes + 1 : 0;
	if (idle && (lowHeapStrikes >= 3 || ms > kMaxUptimeMs)) {
		LOG("Restarting for maintenance (heap %u, uptime %lu s).", ESP.getFreeHeap(), (unsigned long)(ms / 1000));
		delay(100);
		ESP.restart();
	}
}
}  // namespace

void setup() {
	actuators.begin();  // outputs low before anything else
	Serial.begin(115200);
	delay(200);
	LOG("\nDoora | po/iw hackerspace | https://github.com/poiw-org/doora");

	Persist::begin();
	if (!SettingsStore::load(settings)) {
		Console::firstRunSetup(settings);
		LOG("Restarting with the new settings.");
		delay(200);
		ESP.restart();
	}
	LOG("Door \"%s\". Type `help` for console commands.", settings.doorId);

	clock_.begin(settings.ntpServer);
	if (Persist::loadBlacklist(blacklist)) {
		blacklist.prune(clock_.now());
		LOG("Blacklist restored from flash: %u disabled keys.", (unsigned)blacklist.size());
	} else {
		LOG("No usable blacklist in flash; it will be fetched when the network is up.");
	}
	LOG("%u events waiting to be uploaded.", (unsigned)Persist::journalCount());

	reader.begin();
	Cloud::Hooks hooks = {onForceOpen};
	cloud.begin(settings, clock_, blacklist, hooks);
}

void loop() {
	uint32_t ms = millis();

	if (reader.poll(tag)) {
		actuators.reading(true);
		handleTag();
		actuators.reading(false);
	}
	actuators.setOnline(cloud.online());
	actuators.service();

	bool quiet = !actuators.busy() && millis() - lastTapMs > kQuietAfterTapMs;
	cloud.service(quiet);
	clock_.service();
	housekeeping(ms);
	if (Console::poll(settings, printStatus)) {
		LOG("Settings changed; restarting.");
		delay(200);
		ESP.restart();
	}
}
