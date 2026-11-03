// Doora | po/iw hackerspace | https://github.com/poiw-org/doora
//
// Door controller. Taps are decided locally from the signed token on the tag, the clock and a
// persisted blacklist, so the door keeps working through Wi-Fi, internet and power outages.
// Supabase is used to keep that local state fresh and to report what happened. See README.md.
#include <Arduino.h>
#include "Access.h"
#include "Actuators.h"
#include "BlacklistSet.h"
#include "Clock.h"
#include "Cloud.h"
#include "Console.h"
#include "Journal.h"
#include "Jwt.h"
#include "Log.h"
#include "Persist.h"
#include "Pins.h"
#include "Reader.h"
#include "Settings.h"

namespace {
Settings settings;
Clock clock_;
BlacklistSet blacklist;
Actuators actuators;
Reader reader;
Cloud cloud;

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

void handleTag(const TagRead& tag) {
	lastTapMs = millis();
	JournalRecord r = newRecord();
	r.uidLen = tag.uidLen;
	memcpy(r.uid, tag.uid, tag.uidLen);

	KeyClaims key;
	JwtResult parsed = tag.token ? jwtVerify(tag.token, tag.tokenLen, (const uint8_t*)settings.jwtSecret,
	                                            strlen(settings.jwtSecret), key)
	                                : JwtResult::Malformed;
	if (parsed != JwtResult::Ok) {
		actuators.deny();
		actuators.service();
		r.verdict = 255;  // no Verdict applies; stored as reason "Unknown"
		r.flags |= kInvalidToken;
		static const char* const kWhy[] = {"ok", "malformed token", "bad signature (check the signing secret)", "missing or bad claims"};
		LOG("Denied: the tag carries no valid key (%s).", tag.token ? kWhy[(int)parsed] : "no token found on the tag");
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
		actuators.service();  // energize the relay now, before the flash write in record()
		r.allowed = 1;
		LOG("Door opened for key %s.", key.jti);
		record(r);
	} else {
		actuators.deny();
		actuators.service();
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
	    ESP.getMaxAllocHeap());
	LOG("clock     : %lu (%s)", (unsigned long)clock_.now(), clock_.synced() ? "synced" : "estimated");
	LOG("blacklist : %u disabled keys", (unsigned)blacklist.size());
	LOG("journal   : %u events waiting, %lu dropped", (unsigned)Persist::journalCount(),
	    (unsigned long)Persist::journalDropped());
	reader.printStatus();
	cloud.printStatus();
}

// Periodic upkeep that never matters to a tap.
void housekeeping(uint32_t ms) {
	if (ms - lastHousekeepingMs < 60000) return;
	lastHousekeepingMs = ms;

	blacklist.prune(clock_.now());

	bool idle = !actuators.busy() && ms - lastTapMs > 60000;
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

	reader.begin(NFC_SDA, NFC_SCL);
	Cloud::Hooks hooks = {onForceOpen};
	cloud.begin(settings, clock_, blacklist, hooks);
}

void loop() {
	uint32_t ms = millis();

	if (const TagRead* tag = reader.take()) {
		actuators.reading(true);
		handleTag(*tag);
		actuators.reading(false);
		reader.release();
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
