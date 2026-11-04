#include "Cloud.h"
#include <ArduinoJson.h>
#include "Journal.h"
#include "Jwt.h"
#include "Log.h"
#include "Net.h"
#include "Persist.h"
#include "SyncParser.h"
#include "TimeUtil.h"

namespace {
constexpr uint32_t kHttpTimeoutMs = 8000;
constexpr uint32_t kRestPollMs = 250;
constexpr uint32_t kSyncEveryMs = 300000;     // safety net for missed realtime events
constexpr uint8_t kMaxSupersededRuns = 3;      // overtaken syncs in a row before one is applied anyway
constexpr uint32_t kMinSyncGapMs = 30000;     // a sync asked for by a reconnect never follows another this closely
constexpr uint32_t kKeyRefreshMs = 6UL * 3600 * 1000;  // the public key only changes if the seed does
constexpr uint32_t kPersistDelayMs = 2000;    // a burst of key changes is written to flash once
constexpr uint32_t kRejectedHoldMs = 300000;  // the door account is refused; the settings need fixing

constexpr uint32_t kTaskStack = 10240;  // handshake plus the sign-in buffers; `status` shows the headroom
constexpr UBaseType_t kTaskPriority = 1;
constexpr BaseType_t kTaskCore = 0;  // with Wi-Fi and lwIP, away from the NFC reader and loop()

bool timeReached(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

// Reads a response body for ArduinoJson. While the server is slow it sleeps in select() instead
// of spinning, which would starve the idle task of core 0 and trip the watchdog.
class BodyReader : public Stream {
 public:
	using Stream::readBytes;
	BodyReader(WiFiClientSecure& tls, uint32_t deadlineMs) : tls_(tls), deadline_(deadlineMs) {}
	int available() override { return tls_.available(); }
	int read() override {
		char c;
		return readBytes(&c, 1) == 1 ? (unsigned char)c : -1;
	}
	int peek() override { return -1; }
	size_t write(uint8_t) override { return 0; }
	size_t readBytes(char* buffer, size_t length) override {
		size_t got = 0;
		while (got < length) {
			int avail = tls_.available();
			if (avail > 0) {
				size_t want = (size_t)avail < length - got ? (size_t)avail : length - got;
				int n = tls_.read(reinterpret_cast<uint8_t*>(buffer) + got, want);
				if (n > 0) got += (size_t)n;
				continue;
			}
			int32_t left = (int32_t)(deadline_ - millis());
			if (left <= 0 || !tls_.connected()) break;
			Net::waitReadable(tls_, left < 100 ? (uint32_t)left : 100);
		}
		return got;
	}

 private:
	WiFiClientSecure& tls_;
	uint32_t deadline_;
};

// Reads one header line; false on timeout or a closed connection.
bool readLine(WiFiClientSecure& c, char* out, size_t cap, uint32_t deadline) {
	size_t n = 0;
	for (;;) {
		int32_t left = (int32_t)(deadline - millis());
		if (left <= 0) return false;
		if (c.available() <= 0) {
			if (!c.connected()) return false;
			Net::waitReadable(c, left < 100 ? (uint32_t)left : 100);
			continue;
		}
		int ch = c.read();
		if (ch == '\n') { out[n] = 0; return true; }
		if (ch != '\r' && ch >= 0 && n + 1 < cap) out[n++] = (char)ch;
	}
}
}  // namespace

void Cloud::begin(const Settings& settings, Clock& clock, BlacklistSet& blacklist, SigningKey& signingKey) {
	cfg_ = &settings;
	clock_ = &clock;
	blacklist_ = &blacklist;
	signingKey_ = &signingKey;

	char name[48];
	snprintf(name, sizeof(name), "doora-%s", settings.doorId);
	WiFi.persistent(false);
	WiFi.setHostname(name);  // must precede mode()
	WiFi.mode(WIFI_STA);
	WiFi.setAutoReconnect(true);
	WiFi.setSleep(false);  // mains powered; sleeping only adds latency to commands
	WiFi.begin(settings.wifiSsid, settings.wifiPass[0] ? settings.wifiPass : nullptr);
	wifiBeginMs_ = wifiKickMs_ = millis();
	LOG("Connecting to Wi-Fi \"%s\"...", settings.wifiSsid);

	realtime_.begin(settings, clock, blacklist, token_, events_);
	xTaskCreatePinnedToCore(&Cloud::runRest, "rest", kTaskStack, this, kTaskPriority, &restTask_, kTaskCore);
}

void Cloud::service() { manageWifi(millis()); }

void Cloud::manageWifi(uint32_t ms) {
	if (WiFi.status() == WL_CONNECTED) {
		if (!wifiWasUp_) {
			wifiWasUp_ = true;
			LOG("Wi-Fi up, IP %s.", WiFi.localIP().toString().c_str());
		}
		return;
	}
	if (wifiWasUp_) {
		wifiWasUp_ = false;
		LOG("Wi-Fi lost; running from local state.");
	}
	// The core reconnects on its own; these nudges cover a router that came back in a bad state.
	if (ms - wifiBeginMs_ > 600000) {
		wifiBeginMs_ = wifiKickMs_ = ms;
		WiFi.disconnect();
		WiFi.begin(cfg_->wifiSsid, cfg_->wifiPass[0] ? cfg_->wifiPass : nullptr);
	} else if (ms - wifiKickMs_ > 30000) {
		wifiKickMs_ = ms;
		WiFi.reconnect();
	}
}

// ---------------------------------------------------------------------------------------- REST

int Cloud::request(const char* method, const char* path, const char* body, size_t bodyLen, const char* prefer,
                   bool authed) {
	if (!Net::connect(tls_, cfg_->supabaseHost, kHttpTimeoutMs)) return -1;

	// The whole header goes out in one write, the body in a second.
	size_t cap = sizeof(header_);
	int n = snprintf(header_, cap, "%s %s HTTP/1.0\r\nHost: %s\r\napikey: %s", method, path, cfg_->supabaseHost,
	                 cfg_->anonKey);
	if (n > 0 && authed) {
		n += snprintf(header_ + n, cap - (size_t)n, "\r\nAuthorization: Bearer ");
		if ((size_t)n + 16 >= cap || !token_.copyTo(header_ + n, cap - (size_t)n - 16)) {
			finish();
			return -1;
		}
		n += (int)strlen(header_ + n);
	}
	if (n > 0)
		n += snprintf(header_ + n, cap - (size_t)n, "\r\nContent-Type: application/json\r\nConnection: close\r\n%s%s%sContent-Length: %u\r\n\r\n",
		              prefer ? "Prefer: " : "", prefer ? prefer : "", prefer ? "\r\n" : "", (unsigned)bodyLen);
	if (n <= 0 || (size_t)n >= cap || tls_.write(reinterpret_cast<const uint8_t*>(header_), (size_t)n) != (size_t)n ||
	    (bodyLen && tls_.write(reinterpret_cast<const uint8_t*>(body), bodyLen) != bodyLen)) {
		finish();
		return -1;
	}

	uint32_t deadline = millis() + kHttpTimeoutMs;
	char line[128];
	if (!readLine(tls_, line, sizeof(line), deadline) || strncmp(line, "HTTP/", 5) != 0) {
		finish();
		return -1;
	}
	const char* space = strchr(line, ' ');
	int code = space ? atoi(space + 1) : -1;
	while (readLine(tls_, line, sizeof(line), deadline)) {
		if (!line[0]) return code;
	}
	finish();
	return -1;
}

// Remembers what a status code means for later attempts.
void Cloud::noteStatus(int code) {
	if (code == 401) {
		token_.clear();  // expired or revoked: sign in again
	} else if (code == 403 || code == 404) {
		if (!authRejected_) LOG("Supabase refused the door account (HTTP %d). Is it listed in public.door_users?", code);
		authRejected_ = true;
	} else if (code >= 200 && code < 300) {
		authRejected_ = false;
	}
}

bool Cloud::grantToken(bool useRefresh) {
	char body[460];
	const char* path;
	if (useRefresh) {
		char esc[2 * sizeof(refresh_)];
		if (!jsonEscape(refresh_, esc, sizeof(esc))) return false;
		snprintf(body, sizeof(body), "{\"refresh_token\":\"%s\"}", esc);
		path = "/auth/v1/token?grant_type=refresh_token";
	} else {
		char email[140], password[140];
		if (!jsonEscape(cfg_->email, email, sizeof(email)) || !jsonEscape(cfg_->password, password, sizeof(password)))
			return false;
		snprintf(body, sizeof(body), "{\"email\":\"%s\",\"password\":\"%s\"}", email, password);
		path = "/auth/v1/token?grant_type=password";
	}

	int code = request("POST", path, body, strlen(body), nullptr, false);
	memset(body, 0, sizeof(body));  // contains the password
	bool ok = false;
	if (code == 200) {
		JsonDocument filter;
		filter["access_token"] = true;
		filter["refresh_token"] = true;
		filter["expires_in"] = true;
		JsonDocument doc;
		BodyReader reader(tls_, millis() + kHttpTimeoutMs);
		DeserializationError err = deserializeJson(doc, reader, DeserializationOption::Filter(filter));
		const char* token = doc["access_token"];
		if (!err && token && token_.set(token)) {
			const char* renew = doc["refresh_token"];
			if (renew && strlen(renew) < sizeof(refresh_)) strlcpy(refresh_, renew, sizeof(refresh_));
			uint32_t lifetime = doc["expires_in"] | 3600;
			// Renew at 75% of the lifetime: late enough to be cheap, early enough to survive a failed try.
			refreshAtMs_ = millis() + lifetime * 750UL;
			authRejected_ = false;
			ok = true;
			LOG("Signed in as the door account.");
		} else {
			LOG("Unreadable sign-in response (%s).", err.c_str());
		}
	} else if (code == 400 || code == 401 || code == 422) {
		if (useRefresh) {
			refresh_[0] = 0;  // fall back to the password
		} else {
			authRejected_ = true;
			LOG("Sign-in refused (HTTP %d). Check the door email and password with the `config` command.", code);
		}
	} else {
		LOG("Sign-in failed (HTTP %d).", code);
	}
	finish();
	return ok;
}

// Signs in when there is no token, and renews it ahead of its expiry. A failed renewal is
// harmless while the old token is still valid: the other work continues with it.
void Cloud::refreshSession(uint32_t ms) {
	if (token_.present() && !timeReached(ms, refreshAtMs_)) return;
	if (!authBackoff_.due(ms)) return;
	if ((refresh_[0] && grantToken(true)) || grantToken(false)) {
		authBackoff_.succeed();
		return;
	}
	if (authRejected_) {
		authBackoff_.hold(ms, kRejectedHoldMs);
		LOG("Sign-in will be retried in %lu s.", (unsigned long)(kRejectedHoldMs / 1000));
	} else {
		LOG("Cloud unavailable; retrying sign-in in %lu s.", (unsigned long)((authBackoff_.fail(ms, esp_random()) + 999) / 1000));
	}
}

bool Cloud::syncDue(uint32_t ms) const {
	if (!syncBackoff_.due(ms)) return false;
	if (!everSynced_ || urgent_) return true;
	uint32_t since = ms - lastSyncMs_;
	return since >= kSyncEveryMs || (events_.resync.load(std::memory_order_acquire) && since >= kMinSyncGapMs);
}

// Replaces the local blacklist with the server's list of disabled, unexpired keys. The new list is
// built on the side, so a download that fails halfway keeps the old one.
bool Cloud::syncBlacklist() {
	// Cleared first: a request made while the download runs asks for another round.
	events_.resync.store(false, std::memory_order_release);
	urgent_ = false;
	staging_.clear();
	uint32_t edits = blacklist_->edits();

	int code = request("POST", "/rest/v1/rpc/door_sync", "{}", 2, nullptr, true);
	bool ok = false, superseded = false;
	if (code == 200) {
		SyncParser parser(staging_);
		char chunk[256];
		uint32_t deadline = millis() + 2 * kHttpTimeoutMs;
		while (!parser.complete() && !timeReached(millis(), deadline) && (tls_.connected() || tls_.available())) {
			int n = tls_.available();
			if (n <= 0) {
				Net::waitReadable(tls_, 100);
				continue;
			}
			if (n > (int)sizeof(chunk)) n = sizeof(chunk);
			n = tls_.read((uint8_t*)chunk, n);
			if (n > 0) parser.feed(chunk, (size_t)n);
		}
		if (parser.complete()) {
			// The server time comes over TLS: it corrects the clock, and it decides what has expired.
			clock_->adoptServerTime(parser.now());
			if (parser.now()) staging_.prune(parser.now());
			truncated_ = parser.truncated();
			// A key change that arrived by realtime during the download is newer than this snapshot,
			// so the snapshot only replaces the live list if nothing edited it in the meantime.
			// After a few overtaken rounds in a row (a stream of key edits) the snapshot is applied
			// anyway and another sync follows after the minimum gap, so key edits can never starve the
			// full sync that repairs missed realtime events.
			bool forced = false;
			bool applied = blacklist_->replaceWithIfUnedited(staging_, edits);
			if (!applied && supersededRuns_ >= kMaxSupersededRuns) {
				blacklist_->replaceWith(staging_);
				applied = forced = true;
			}
			if (applied) {
				supersededRuns_ = 0;
				events_.blacklistChanged.store(true, std::memory_order_release);
				ok = true;
				if (forced) events_.resync.store(true, std::memory_order_release);
				LOG("Blacklist synced: %u disabled keys%s.", (unsigned)blacklist_->size(),
				    truncated_ ? " (the server has more than fit; the newest are kept)" : "");
			} else {
				supersededRuns_++;
				superseded = true;
				LOG("A key changed during the blacklist download; fetching it again.");
			}
		} else {
			LOG("Blacklist download was incomplete; keeping the stored list.");
		}
	} else {
		LOG("Blacklist sync failed (HTTP %d).", code);
	}
	noteStatus(code);
	finish();

	uint32_t ms = millis();
	if (ok) {
		everSynced_ = true;
		lastSyncMs_ = ms;
		syncBackoff_.succeed();
	} else if (superseded) {
		// Not a failure: retry at once, without the backoff or the minimum gap between syncs.
		events_.resync.store(true, std::memory_order_release);
		urgent_ = true;
	} else {
		events_.resync.store(true, std::memory_order_release);
		syncBackoff_.fail(ms, esp_random());
	}
	return ok;
}

// Learns the public key that verifies v2 key tokens. The backoffice derives it from its secret seed
// and sends only the modulus (the exponent is always 65537). It is kept in flash, so the door can
// verify v2 keys offline after the first fetch.
bool Cloud::fetchSigningKey() {
	int code = request("GET", "/functions/v1/door-key", "", 0, nullptr, true);
	bool ok = false;
	if (code == 200) {
		JsonDocument filter;
		filter["n"] = true;
		JsonDocument doc;
		BodyReader reader(tls_, millis() + 2 * kHttpTimeoutMs);
		DeserializationError err = deserializeJson(doc, reader, DeserializationOption::Filter(filter));
		const char* text = doc["n"];
		uint8_t n[kRsaBytes + 1];
		int len = (!err && text) ? base64UrlDecode(text, strlen(text), n, sizeof(n)) : -1;
		if (len == (int)kRsaBytes && signingKey_->sameAs(n)) {
			ok = true;
		} else if (len == (int)kRsaBytes && signingKey_->set(n)) {
			ok = true;
			LOG("Received a new public key for v2 keys.");
			if (!Persist::saveSigningKey(n)) LOG("Could not store the public key; it is kept until reboot.");
		} else {
			LOG("Unreadable public key response.");
		}
	} else {
		LOG("Public key fetch failed (HTTP %d).", code);
	}
	noteStatus(code);
	finish();
	if (ok) {
		keyFetched_ = true;
		keyFetchedMs_ = millis();
		keyBackoff_.succeed();
	} else {
		keyBackoff_.fail(millis(), esp_random());
	}
	return ok;
}

// Uploads queued door events in small batches through the door_log function. Events leave the
// queue only after the server accepted them; the function ignores events it already stored, so a
// batch whose reply was lost can simply be sent again.
bool Cloud::flushJournal() {
	bool ok = true;
	while (ok && Persist::journalCount()) {
		JournalRecord records[kFlushBatch];
		uint32_t seq;
		size_t n = Persist::journalRead(records, kFlushBatch, &seq);
		if (!n) break;

		size_t len = 0, valid = 0;
		len += snprintf(body_, 16, "{\"entries\":[");
		for (size_t i = 0; i < n; i++) {
			if (!journalValid(records[i])) continue;  // damaged by flash corruption; dropped below
			if (valid) body_[len++] = ',';
			size_t w = journalToJson(records[i], cfg_->doorId, body_ + len, kRecordJson);
			if (!w) { if (valid) len--; continue; }
			len += w;
			valid++;
		}
		body_[len++] = ']';
		body_[len++] = '}';
		body_[len] = 0;

		int code = 204;
		if (valid) {
			code = request("POST", "/rest/v1/rpc/door_log", body_, len, nullptr, true);
			noteStatus(code);
			finish();
		}

		bool accepted = code >= 200 && code < 300;
		// 400/413/422 mean the batch itself is unacceptable; retrying would block the queue forever.
		bool poison = code == 400 || code == 413 || code == 422;
		if (poison) LOG("Server rejected %u events (HTTP %d); discarding them.", (unsigned)n, code);
		if (accepted || poison) {
			if (!Persist::journalDrop(n, seq)) ok = false;
		} else {
			LOG("Event upload failed (HTTP %d); %u queued.", code, (unsigned)Persist::journalCount());
			ok = false;
		}
	}
	if (ok) journalBackoff_.succeed();
	else journalBackoff_.fail(millis(), esp_random());
	return ok;
}

// Key changes arrive in bursts (an admin disabling several keys), so the flash is written once
// the burst is over.
void Cloud::persistBlacklistIfDirty(uint32_t ms) {
	if (events_.blacklistChanged.load(std::memory_order_acquire) && !dirtySeen_) {
		dirtySeen_ = true;
		dirtySeenMs_ = ms;
	}
	if (!dirtySeen_ || ms - dirtySeenMs_ < kPersistDelayMs) return;
	dirtySeen_ = false;
	events_.blacklistChanged.store(false, std::memory_order_release);  // a change during the write sets it again
	if (!Persist::saveBlacklist(*blacklist_)) {
		LOG("Could not store the blacklist; will retry.");
		events_.blacklistChanged.store(true, std::memory_order_release);
	}
}

void Cloud::runRest(void* self) { static_cast<Cloud*>(self)->restLoop(); }

void Cloud::restLoop() {
	for (;;) {
		vTaskDelay(pdMS_TO_TICKS(kRestPollMs));
		if (WiFi.status() != WL_CONNECTED) continue;
		uint32_t ms = millis();

		refreshSession(ms);
		if (token_.present()) {
			if (Persist::journalCount() && journalBackoff_.due(millis())) flushJournal();
			if (syncDue(millis())) syncBlacklist();
			uint32_t now = millis();
			if (keyBackoff_.due(now) && (!keyFetched_ || now - keyFetchedMs_ >= kKeyRefreshMs)) fetchSigningKey();
		}
		persistBlacklistIfDirty(millis());
	}
}

void Cloud::printStatus() {
	LOG("wifi      : %s (%d dBm)", WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "down", WiFi.RSSI());
	realtime_.printStatus();
	LOG("token     : %s", token_.present() ? "valid" : "none");
	LOG("tls       : %lu low-memory refusals, rest task stack headroom %u of %u bytes",
	    (unsigned long)Net::lowMemoryRefusals(), (unsigned)uxTaskGetStackHighWaterMark(restTask_), (unsigned)kTaskStack);
	LOG("public key: %s", signingKey_->present() ? (keyFetched_ ? "fetched" : "from flash") : "none");
	LOG("last sync : %s", everSynced_ ? "done" : "never");
	if (truncated_ || blacklist_->overflowed())
		LOG("blacklist : FULL, some disabled keys are not blocked at this door (%s)",
		    truncated_ ? "the server list was cut short" : "realtime changes evicted entries");
}
