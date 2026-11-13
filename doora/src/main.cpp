// Doora | po/iw hackerspace | https://github.com/poiw-org/doora
//
// Door controller. Taps are decided locally from the signed token on the tag, the clock and a
// persisted blacklist, so the door keeps working through Wi-Fi, internet and power outages.
// Supabase is used to keep that local state fresh and to report what happened. Network work runs
// in tasks of its own (Cloud.h), so this loop only handles taps, the relay and the console.
// See README.md.
#include <Arduino.h>
#include "Access.h"
#include "Actuators.h"
#include "BleProto.h"
#include "BlacklistSet.h"
#include "Bridge.h"
#include "Bt.h"
#include "Clock.h"
#include "Chimes.h"
#include "Cloud.h"
#include "Console.h"
#include "Journal.h"
#include "Jwt.h"
#include "Log.h"
#include "Ota.h"
#include "Persist.h"
#include "Pins.h"
#include "HardRestart.h"
#include "Reader.h"
#include "ResetGuard.h"
#include "Settings.h"
#include "SigningKey.h"

namespace {
Settings settings;
Clock clock_;
BlacklistSet blacklist;
SigningKey signingKey;
Actuators actuators;
Reader reader;
Bridge bridge;
Cloud cloud;

uint32_t lastTapMs = 0;
uint32_t lastFailureLogMs = 0;
uint32_t lastHousekeepingMs = 0;
uint8_t lowHeapStrikes = 0;

constexpr uint32_t kFailureLogGapMs = 3000;     // rate limit for logging denied taps
constexpr uint32_t kMaxUptimeMs = 7UL * 24 * 3600 * 1000;
constexpr uint32_t kLowHeapBytes = 7000;
// The PN532 has not answered for this long, including its own reset and retries in the reader task.
constexpr uint32_t kNfcDownResetMs = 10UL * 60 * 1000;

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

// Called once the force-open countdown ended and the relay is already energised.
// v1 tokens are checked against the shared secret, v2 tokens against the backoffice's public key.
JwtResult verifyToken(const char* token, size_t len, KeyClaims& key) {
	RsaPublicKey rsa;
	VerifyKeys keys{(const uint8_t*)settings.jwtSecret, strlen(settings.jwtSecret), signingKey.copyTo(rsa) ? &rsa : nullptr};
	return jwtVerify(token, len, keys, key);
}

// BLE credentials are RS256 only: the v1 shared secret never vouches for a phone.
JwtResult verifyBleToken(const char* token, size_t len, KeyClaims& key) {
	RsaPublicKey rsa;
	VerifyKeys keys{nullptr, 0, signingKey.copyTo(rsa) ? &rsa : nullptr};
	return jwtVerify(token, len, keys, key);
}

void onForcedOpened() {
	lastTapMs = millis();
	JournalRecord r = newRecord();
	r.allowed = 1;
	r.flags |= kForced;
	record(r);
	LOG("Door opened (force open).");
}

// Writer mode: the backoffice asks the door to write a key to a tag. The token must carry the
// door's own signature, so the reader can never be used to write anything else. The tag it is
// written to is the one named in the token's audience.
void serviceWriter() {
	static WriteState reported = WriteState::Idle;
	static bool scanning = false;  // the session reads a serial number instead of writing
	if (const auto* req = cloud.writeRequest()) {
		KeyClaims key;
		size_t len = strlen(req->token);
		scanning = len == 0;
		bool ok = scanning ? reader.armScan(kWriteWindowMs)
		                   : verifyToken(req->token, len, key) == JwtResult::Ok &&
		                         reader.arm(req->token, len, key.aud, key.audLen, kWriteWindowMs);
		if (ok) {
			lastTapMs = millis();
			reported = WriteState::Armed;
			cloud.reportWrite(WriteState::Armed);
			LOG("Writer mode: present the tag within %lu s.", (unsigned long)(kWriteWindowMs / 1000));
		} else {
			cloud.reportWrite(WriteState::Rejected);
			LOG("Writer mode refused: the token is not one this door signed.");
		}
		cloud.releaseWriteRequest();
	}
	if (cloud.takeWriteCancel()) reader.cancelWrite();

	WriteState now = reader.writeState();
	if (now != reported) {
		reported = now;
		if (now == WriteState::Done && scanning) {
			uint8_t uid[10];
			uint8_t n = reader.scannedUid(uid);
			char text[32] = {0};
			size_t at = 0;
			for (uint8_t i = 0; i < n; i++) at += snprintf(text + at, sizeof(text) - at, i ? ":%02x" : "%02x", uid[i]);
			cloud.setScanSerial(text);
			LOG("Scan: tag %s.", text);
		}
		if (now != WriteState::Idle) cloud.reportWrite(now);
	}
	actuators.writer(now);
	actuators.reading(writeActive(now));
}

void handleTag(const TagRead& tag) {
	lastTapMs = millis();
	JournalRecord r = newRecord();
	r.uidLen = tag.uidLen;
	memcpy(r.uid, tag.uid, tag.uidLen);

	KeyClaims key;
	JwtResult parsed = tag.token ? verifyToken(tag.token, tag.tokenLen, key) : JwtResult::Malformed;
	if (parsed != JwtResult::Ok) {
		if (Actuators::kSound) reader.holdOff(Chimes::kDeniedMs + 100);
		actuators.deny();
		actuators.service();
		r.verdict = 255;  // no Verdict applies; stored as reason "Unknown"
		r.flags |= kInvalidToken;
		KeyClaims claimed;
		if (tag.token && jwtPeek(tag.token, tag.tokenLen, claimed) == JwtResult::Ok) journalSetClaims(r, claimed, false);
		static const char* const kWhy[] = {"ok", "malformed token", "bad signature (check the signing secret)", "missing or bad claims", "v2 key but the door has not received the public key yet"};
		LOG("Denied: the tag carries no valid key (%s).", tag.token ? kWhy[(int)parsed] : "no token found on the tag");
		if (millis() - lastFailureLogMs >= kFailureLogGapMs || !lastFailureLogMs) {
			lastFailureLogMs = millis();
			record(r);
		}
		return;
	}

	journalSetClaims(r, key, true);

	Verdict v = decide(key, tag.uid, tag.uidLen, r.ts, clock_.synced(), blacklist.contains(key.jti));
	if (v == Verdict::Allow) {
		actuators.grant();
		actuators.service();  // energize the relay now, before the flash write in record()
		r.allowed = 1;
		LOG("Door opened for key %s, %lu ms after the tag was found.", key.jti, (unsigned long)(millis() - tag.foundMs));
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

// A phone's request, relayed by the bridge. Judged here, never by the bridge (docs/ble-protocol.md,
// section 4): the credential's signature, then the phone's proof over this door's id and the nonce
// the door made, then the rules every key shares. Every request gets exactly one answer.
void handleBle(const BleRequestSlot& req) {
	lastTapMs = millis();
	JournalRecord r = newRecord();
	r.flags |= kBle;

	BleRequest parsed;
	KeyClaims key;
	BleResult result = BleResult::Malformed;
	JwtResult jwt = JwtResult::Malformed;
	if (bleParseRequest(req.body, req.len, parsed)) {
		jwt = verifyBleToken(parsed.jwt, parsed.jwtLen, key);
		switch (jwt) {
			case JwtResult::Ok:
				result = decideBle(key, settings.doorId, bridge.nonce(), parsed.sig, r.ts, clock_.synced(),
				                   blacklist.contains(key.jti));
				break;
			case JwtResult::NoPublicKey: result = BleResult::NoDoorKey; break;
			case JwtResult::Malformed: result = BleResult::Malformed; break;
			default: result = BleResult::BadCredential; break;
		}
	}

	if (jwt == JwtResult::Ok) {
		journalSetClaims(r, key, true);
	} else if (jwt == JwtResult::BadSignature || jwt == JwtResult::BadClaims) {
		KeyClaims claimed;
		if (jwtPeek(parsed.jwt, parsed.jwtLen, claimed) == JwtResult::Ok) journalSetClaims(r, claimed, false);
	}

	if (result == BleResult::Opened) {
		actuators.grant();
		actuators.service();  // energize the relay now, before the UART answer and the flash write
		bridge.reply(result, millis());
		bridge.countOpened();
		r.allowed = 1;
		LOG("Door opened for key %s over Bluetooth.", key.jti);
		record(r);
		return;
	}
	actuators.deny();
	actuators.service();
	bridge.reply(result, millis());
	bridge.countDenied();
	r.verdict = journalVerdict(result);
	if (r.verdict == 255) r.flags |= kInvalidToken;
	static const char* const kWhy[] = {"opened", "malformed request", "bad credential", "the phone's proof failed",
	                                   "key disabled", "key expired", "key not yet valid", "no public key yet", "busy"};
	LOG("Bluetooth request denied: %s.", kWhy[(int)result]);
	if (millis() - lastFailureLogMs >= kFailureLogGapMs || !lastFailureLogMs) {
		lastFailureLogMs = millis();
		record(r);
	}
}

void printStatus() {
	LOG("--- status ---");
	LOG("uptime    : %lu s, free heap %u (lowest %u), largest block %u", (unsigned long)(millis() / 1000),
	    ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
	LOG("clock     : %lu (%s)", (unsigned long)clock_.now(), clock_.synced() ? "synced" : "estimated");
	LOG("blacklist : %u disabled keys", (unsigned)blacklist.size());
	LOG("journal   : %u events waiting, %lu dropped, %lu denied taps not recorded", (unsigned)Persist::journalCount(),
	    (unsigned long)Persist::journalDropped(), (unsigned long)Persist::journalSuppressed());
	reader.printStatus();
	bridge.printStatus();
	cloud.printStatus();
	Bt::printStatus();
}

void serviceBle() {
	bridge.service(millis());
	if (const BleRequestSlot* req = bridge.take()) {
		actuators.reading(true);
		handleBle(*req);
		actuators.reading(false);
		bridge.release();
	}
}

void serviceTag() {
	if (const TagRead* tag = reader.take()) {
		actuators.reading(true);
		handleTag(*tag);
		actuators.reading(false);
		reader.release();
	}
}

// The door's own work, for code that has to wait outside loop(): a console prompt or a firmware
// upload must not hold the relay energised, a force open back or a tap unanswered, and the
// Bluetooth deadlines keep running.
void serviceDoor() {
	serviceTag();
	serviceBle();
	Bt::service(cloud);
	if (cloud.takeForceOpen()) actuators.forceOpen();
	actuators.service();
	if (actuators.takeForcedOpen()) onForcedOpened();
}

// Restarts the door to recover from `why`, unless it already did so kMaxSelfResets times in the
// last 24 hours: a fault a restart does not fix must not become a reboot loop. The restart is
// recorded in flash first, and skipped if that fails, so the cap cannot be bypassed.
void restartToRecover(const char* why) {
	static bool capLogged = false;
	ResetLog log;
	Persist::loadResets(log);
	uint32_t now = clock_.now();
	if (!resetAllowed(log, now)) {
		if (!capLogged) LOG("%s, but %u recovery restarts already happened in 24 h; not restarting.", why, (unsigned)kMaxSelfResets);
		capLogged = true;
		return;
	}
	resetRecord(log, now);
	if (!Persist::saveResets(log)) {
		if (!capLogged) LOG("%s, but the restart could not be recorded; not restarting.", why);
		capLogged = true;
		return;
	}
	LOG("%s; restarting to recover.", why);
	delay(100);
	hardRestart();
}

// Periodic upkeep that never matters to a tap.
void housekeeping(uint32_t ms) {
	if (ms - lastHousekeepingMs < 60000) return;
	lastHousekeepingMs = ms;

	// A time from SNTP alone could be forged into the future; pruning by it would unblock keys.
	if (clock_.reliable()) blacklist.prune(clock_.now());

	bool idle = !actuators.busy() && !writeActive(reader.writeState()) && ms - lastTapMs > 60000;
	// A leak or fragmentation would eventually stop TLS from working. Everything that matters is
	// persisted, so a restart in an idle moment costs nothing.
	lowHeapStrikes = ESP.getFreeHeap() < kLowHeapBytes ? lowHeapStrikes + 1 : 0;
	if (idle && (lowHeapStrikes >= 3 || ms > kMaxUptimeMs)) {
		LOG("Restarting for maintenance (heap %u, uptime %lu s).", ESP.getFreeHeap(), (unsigned long)(ms / 1000));
		delay(100);
		ESP.restart();
	}
	if (idle && reader.downForMs() > kNfcDownResetMs) restartToRecover("PN532 not answering");
}
}  // namespace

// `pitch [x]` and `play [chime]`, from serial or the portal.
bool soundCommand(const char* line) {
	static const struct { const char* name; Actuators::Chime chime; } kChimes[] = {
	    {"unlock", Actuators::Chime::Unlock},
	    {"deny", Actuators::Chime::Deny},
	    {"alert", Actuators::Chime::Alert},
	    {"write", Actuators::Chime::Write},
	};
	if (!strcmp(line, "pitch") || !strncmp(line, "pitch ", 6)) {
		if (line[5]) {
			char* end;
			float x = strtof(line + 6, &end);
			if (end == line + 6 || *end || !(x >= 0.25f && x <= 4.0f)) {
				Term.println(F("Usage: pitch <multiplier>, 0.25 to 4 (1 = as written)."));
				return true;
			}
			uint16_t permille = (uint16_t)(x * 1000 + 0.5f);
			if (!Persist::savePitch(permille)) {
				Term.println(F("Could not save the pitch to flash. Nothing was changed."));
				return true;
			}
			actuators.setPitch(permille);
		}
		Term.printf("Pitch multiplier: %u.%03u\n", actuators.pitch() / 1000, actuators.pitch() % 1000);
		return true;
	}
	if (!strcmp(line, "play") || !strncmp(line, "play ", 5)) {
		for (const auto& c : kChimes)
			if (line[4] && !strcmp(line + 5, c.name)) {
				actuators.play(c.chime);
				Term.printf("Playing %s.\n", c.name);
				return true;
			}
		Term.println(F("Usage: play <unlock|deny|alert|write>"));
		return true;
	}
	return false;
}

void setup() {
	actuators.begin();  // outputs low before anything else
	// Four tasks log. With a buffer a log line returns at once instead of waiting for the UART to
	// drain what another task (often on the other core) is printing.
	Serial.setTxBufferSize(1024);
	Serial.begin(115200);
	delay(200);
	LOG("\nDoora | po/iw hackerspace | https://github.com/poiw-org/doora");

	Persist::begin();
	actuators.setPitch(Persist::loadPitch());
	if (!SettingsStore::load(settings)) {
		Console::firstRunSetup(settings);
		LOG("Restarting with the new settings.");
		delay(200);
		ESP.restart();
	}
	LOG("Door \"%s\". Type `help` for console commands.", settings.doorId);

	clock_.begin(settings.ntpServer);
	{
		uint8_t n[kRsaBytes];
		if (Persist::loadSigningKey(n) && signingKey.set(n)) LOG("Public key restored from flash.");
		else LOG("No public key in flash yet; v2 keys work once it is fetched.");
	}
	if (Persist::loadBlacklist(blacklist)) {
		blacklist.prune(clock_.now());
		LOG("Blacklist restored from flash: %u disabled keys.", (unsigned)blacklist.size());
	} else {
		LOG("No usable blacklist in flash; it will be fetched when the network is up.");
	}
	LOG("%u events waiting to be uploaded.", (unsigned)Persist::journalCount());

	// A phone that taps the reader opens this: the app unlocks, or the website offers the app.
	char link[48];
	snprintf(link, sizeof(link), "poiw.org/d/%s", settings.doorId);
	reader.begin(NFC_SDA, NFC_SCL, settings.doorId[0] ? link : "");
	bridge.begin(settings.doorId);
	cloud.begin(settings, clock_, blacklist, signingKey);
	char host[48];
	snprintf(host, sizeof(host), "doora-%s", settings.doorId);  // the Wi-Fi hostname too
	Ota::begin();
	RemoteConsole::begin();
	Bt::begin(host);
}

void loop() {
	uint32_t ms = millis();

	serviceTag();
	serviceBle();
	Bt::service(cloud);
	serviceWriter();
	if (cloud.takeForceOpen()) actuators.forceOpen();
	actuators.service();
	if (actuators.takeForcedOpen()) onForcedOpened();

	cloud.service();
	clock_.service();
	housekeeping(ms);
	if (const char* command = cloud.remoteCommand()) {
		Console::remote(command, settings, printStatus, soundCommand);
		cloud.releaseRemoteCommand();
	}
	Ota::service(!actuators.busy() && !writeActive(reader.writeState()) && ms - lastTapMs > 10000);
	if (Console::poll(settings, printStatus, serviceDoor, soundCommand)) {
		LOG("Settings changed; restarting.");
		delay(200);
		ESP.restart();
	}
	delay(1);  // lets the idle task run; the tasks that wake this loop's work run on their own
}
