#include "Cloud.h"
#include <ArduinoJson.h>
#include <new>
#include "Certs.h"
#include "Journal.h"
#include "Log.h"
#include "Persist.h"
#include "SyncParser.h"
#include "TimeUtil.h"

namespace {
constexpr uint32_t kHttpTimeoutMs = 8000;
constexpr uint32_t kHeartbeatMs = 25000;
constexpr uint32_t kSyncEveryMs = 900000;   // safety net for missed realtime events
constexpr uint32_t kRetryAfterFailureMs = 60000;
constexpr size_t kMaxMessage = 3072;        // larger realtime messages are dropped and trigger a resync
constexpr size_t kFlushBatch = 8;
constexpr size_t kRecordJson = 400;
constexpr uint32_t kMinBlockMfln = 10000;   // free heap block needed per TLS session
constexpr uint32_t kMinBlockFull = 24000;
constexpr uint32_t kCommandMaxAgeMs = 120000;

bool readLine(Client& c, char* out, size_t cap, uint32_t deadline) {
	size_t n = 0;
	for (;;) {
		if ((int32_t)(millis() - deadline) >= 0) return false;
		if (!c.available()) {
			if (!c.connected()) return false;
			delay(2);
			continue;
		}
		int ch = c.read();
		if (ch == '\n') { out[n] = 0; return true; }
		if (ch != '\r' && n + 1 < cap) out[n++] = (char)ch;
	}
}

bool safeId(const char* id) {
	size_t n = strlen(id);
	if (n == 0 || n > 39) return false;
	for (size_t i = 0; i < n; i++)
		if (!isalnum((unsigned char)id[i]) && id[i] != '-' && id[i] != '_') return false;
	return true;
}
}  // namespace

void Cloud::begin(const Settings& settings, Clock& clock, BlacklistSet& blacklist, Hooks hooks) {
	cfg_ = &settings;
	clock_ = &clock;
	blacklist_ = &blacklist;
	hooks_ = hooks;

	for (const char* pem : kRoots) anchors_.append(pem);
	tls_.setTrustAnchors(&anchors_);
	tls_.setSession(&session_);
	tls_.setTimeout(kHttpTimeoutMs);

	WiFi.persistent(false);
	WiFi.mode(WIFI_STA);
	WiFi.setAutoReconnect(true);
	WiFi.setSleepMode(WIFI_NONE_SLEEP);  // mains powered; sleeping only adds latency to commands
	char name[48];
	snprintf(name, sizeof(name), "doora-%s", settings.doorId);
	WiFi.hostname(name);
	WiFi.begin(settings.wifiSsid, settings.wifiPass[0] ? settings.wifiPass : nullptr);
	wifiBeginMs_ = wifiKickMs_ = millis();
	LOG("Connecting to Wi-Fi \"%s\"...", settings.wifiSsid);
}

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

bool Cloud::prepareTls() {
	if (!mflnTried_) {
		mflnTried_ = true;
		// Smaller TLS buffers (if the server agrees) leave room for the rest of the firmware.
		if (tls_.probeMaxFragmentLength(cfg_->supabaseHost, 443, 1024)) {
			tls_.setBufferSizes(1024, 1024);
			mflnOk_ = true;
		}
		LOG("TLS buffers: %s.", mflnOk_ ? "1 KB (server supports it)" : "16 KB");
	}
	uint32_t block = ESP.getMaxFreeBlockSize();
	if (block < (mflnOk_ ? kMinBlockMfln : kMinBlockFull)) {
		lowMemory_++;
		LOG("Not enough contiguous memory for TLS (%lu bytes, %lu times).", (unsigned long)block,
		    (unsigned long)lowMemory_);
		return false;
	}
	tls_.stop();
	// Certificate validity is checked against this; an estimate is fine (certificates last months).
	tls_.setX509Time(clock_->now());
	return true;
}

int Cloud::request(const char* method, const char* path, const char* body, size_t bodyLen, const char* prefer,
                   bool authed) {
	if (!prepareTls()) return -1;
	if (!tls_.connect(cfg_->supabaseHost, 443)) {
		LOG("TLS connect failed (error %d).", tls_.getLastSSLError());
		return -1;
	}
	tls_.print(method);
	tls_.print(' ');
	tls_.print(path);
	tls_.print(F(" HTTP/1.0\r\nHost: "));
	tls_.print(cfg_->supabaseHost);
	tls_.print(F("\r\napikey: "));
	tls_.print(cfg_->anonKey);
	if (authed) {
		tls_.print(F("\r\nAuthorization: Bearer "));
		tls_.print(access_);
	}
	tls_.print(F("\r\nContent-Type: application/json\r\nConnection: close\r\n"));
	if (prefer) {
		tls_.print(F("Prefer: "));
		tls_.print(prefer);
		tls_.print(F("\r\n"));
	}
	tls_.print(F("Content-Length: "));
	tls_.print((unsigned)bodyLen);
	tls_.print(F("\r\n\r\n"));
	if (bodyLen) tls_.write((const uint8_t*)body, bodyLen);
	tls_.flush();

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
		access_[0] = 0;  // expired or revoked: sign in again
	} else if (code == 403 || code == 404) {
		if (!authRejected_) LOG("Supabase refused the door account (HTTP %d). Is it listed in public.door_users?", code);
		authRejected_ = true;
	} else if (code >= 200 && code < 300) {
		authRejected_ = false;
	}
}

bool Cloud::tokenNeedsRefresh(uint32_t ms) const {
	return !access_[0] || (int32_t)(ms - refreshAtMs_) >= 0;
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
		DeserializationError err = deserializeJson(doc, tls_, DeserializationOption::Filter(filter));
		const char* token = doc["access_token"];
		if (!err && token && strlen(token) < sizeof(access_)) {
			strlcpy(access_, token, sizeof(access_));
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

bool Cloud::ensureSession() {
	if (access_[0] && !tokenNeedsRefresh(millis())) return true;
	if (refresh_[0] && grantToken(true)) return true;
	return grantToken(false);
}

// Replaces the local blacklist with the server's list of disabled, unexpired keys. The new list is
// built on the side, so a download that fails halfway keeps the old one.
bool Cloud::syncBlacklist() {
	BlacklistSet* staging = new (std::nothrow) BlacklistSet();
	if (!staging) return false;

	int code = request("POST", "/rest/v1/rpc/door_sync", "{}", 2, nullptr, true);
	bool ok = false;
	if (code == 200) {
		SyncParser parser(*staging);
		char chunk[256];
		uint32_t deadline = millis() + 2 * kHttpTimeoutMs;
		while (!parser.complete() && (int32_t)(millis() - deadline) < 0 && (tls_.connected() || tls_.available())) {
			int n = tls_.available();
			if (n <= 0) { delay(2); continue; }
			if (n > (int)sizeof(chunk)) n = sizeof(chunk);
			n = tls_.read((uint8_t*)chunk, n);
			if (n > 0) parser.feed(chunk, (size_t)n);
		}
		if (parser.complete()) {
			clock_->adoptServerTime(parser.now());
			uint32_t now = clock_->now();
			long skew = (long)now - (long)parser.now();
			if (clock_->synced() && (skew > 60 || skew < -60)) LOG("Clock differs from the server by %ld s.", skew);
			staging->prune(now);
			*blacklist_ = *staging;
			blacklistDirty_ = true;
			ok = true;
			LOG("Blacklist synced: %u disabled keys%s.", (unsigned)blacklist_->size(),
			    parser.truncated() ? " (the server has more than fit; the newest are kept)" : "");
		} else {
			LOG("Blacklist download was incomplete; keeping the stored list.");
		}
	} else {
		LOG("Blacklist sync failed (HTTP %d).", code);
	}
	noteStatus(code);
	finish();
	delete staging;

	if (ok) {
		syncDue_ = false;
		lastSyncMs_ = millis();
		persistBlacklistIfDirty();
	} else {
		syncRetryMs_ = millis() + kRetryAfterFailureMs;
	}
	return ok;
}

// Uploads queued door events in small batches through the door_log function. Events leave the
// queue only after the server accepted them; the function ignores events it already stored, so a
// batch whose reply was lost can simply be sent again.
bool Cloud::flushJournal() {
	char* body = (char*)malloc(kFlushBatch * kRecordJson + 20);
	if (!body) return false;
	bool ok = true;
	while (ok && Persist::journalCount()) {
		JournalRecord records[kFlushBatch];
		size_t n = Persist::journalRead(records, kFlushBatch);
		if (!n) break;

		size_t len = 0, valid = 0;
		len += snprintf(body, 16, "{\"entries\":[");
		for (size_t i = 0; i < n; i++) {
			if (!journalValid(records[i])) continue;  // damaged by flash corruption; dropped below
			if (valid) body[len++] = ',';
			size_t w = journalToJson(records[i], cfg_->doorId, body + len, kRecordJson);
			if (!w) { if (valid) len--; continue; }
			len += w;
			valid++;
		}
		body[len++] = ']';
		body[len++] = '}';
		body[len] = 0;

		int code = 204;
		if (valid)
			code = request("POST", "/rest/v1/rpc/door_log", body, len, nullptr, true);
		if (valid) { noteStatus(code); finish(); }

		bool accepted = code >= 200 && code < 300;
		// 400/413/422 mean the batch itself is unacceptable; retrying would block the queue forever.
		bool poison = code == 400 || code == 413 || code == 422;
		if (poison) LOG("Server rejected %u events (HTTP %d); discarding them.", (unsigned)n, code);
		if (accepted || poison) {
			if (!Persist::journalDrop(n)) ok = false;
		} else {
			LOG("Event upload failed (HTTP %d); %u queued.", code, (unsigned)Persist::journalCount());
			ok = false;
		}
	}
	free(body);
	if (!ok) journalRetryMs_ = millis() + kRetryAfterFailureMs;
	return ok;
}

void Cloud::persistBlacklistIfDirty() {
	if (!blacklistDirty_) return;
	if (Persist::saveBlacklist(*blacklist_)) blacklistDirty_ = false;
}

// ----------------------------------------------------------------------------------- realtime

bool Cloud::openRealtime() {
	if (!prepareTls()) return false;
	char path[320];
	snprintf(path, sizeof(path), "/realtime/v1/websocket?apikey=%s&vsn=1.0.0", cfg_->anonKey);
	if (!ws_.open(cfg_->supabaseHost, path, kHttpTimeoutMs, kMaxMessage)) return false;

	size_t cap = sizeof(access_) + 440;
	char* join = (char*)malloc(cap);
	if (!join) { ws_.close(); return false; }
	uint32_t ref = ++ref_;
	snprintf(join, cap,
	         "{\"topic\":\"realtime:door:%s\",\"event\":\"phx_join\",\"payload\":{\"config\":{"
	         "\"broadcast\":{\"ack\":false,\"self\":false},\"presence\":{\"key\":\"\"},"
	         "\"postgres_changes\":[{\"event\":\"*\",\"schema\":\"public\",\"table\":\"keys\"}],\"private\":true},"
	         "\"access_token\":\"%s\"},\"ref\":\"%lu\",\"join_ref\":\"%lu\"}",
	         cfg_->doorId, access_, (unsigned long)ref, (unsigned long)ref);
	bool sent = ws_.sendText(join, strlen(join));
	free(join);
	if (!sent) { ws_.close(); return false; }

	JsonDocument filter;
	filter["event"] = true;
	filter["ref"] = true;
	filter["payload"]["status"] = true;

	uint32_t deadline = millis() + kHttpTimeoutMs;
	char expected[12];
	snprintf(expected, sizeof(expected), "%lu", (unsigned long)ref);
	while ((int32_t)(millis() - deadline) < 0) {
		WsClient::Event e = ws_.poll();
		if (e == WsClient::Event::Closed) break;
		if (e == WsClient::Event::Message) {
			JsonDocument doc;
			if (!deserializeJson(doc, ws_.text(), ws_.length(), DeserializationOption::Filter(filter)) &&
			    !strcmp(doc["event"] | "", "phx_reply") && !strcmp(doc["ref"] | "", expected)) {
				if (!strcmp(doc["payload"]["status"] | "", "ok")) {
					joined_ = true;
					lastRxMs_ = lastHeartbeatMs_ = millis();
					heartbeatOutstanding_ = false;
					failures_ = 0;
					LOG("Realtime connected (channel door:%s).", cfg_->doorId);
					return true;
				}
				LOG("Realtime join refused. Is the door account listed in public.door_users?");
				access_[0] = 0;
				break;
			}
		}
		delay(5);
	}
	ws_.close();
	return false;
}

void Cloud::dropRealtime(const char* why, bool failure) {
	if (joined_ || ws_.connected()) LOG("Realtime closed: %s.", why);
	ws_.close();
	joined_ = false;
	heartbeatOutstanding_ = false;
	if (failure) backoff(false);
}

void Cloud::sendHeartbeat() {
	char msg[96];
	heartbeatRef_ = ++ref_;
	snprintf(msg, sizeof(msg), "{\"topic\":\"phoenix\",\"event\":\"heartbeat\",\"payload\":{},\"ref\":\"%lu\"}",
	         (unsigned long)heartbeatRef_);
	heartbeatOutstanding_ = true;
	lastHeartbeatMs_ = millis();
	if (!ws_.sendText(msg, strlen(msg))) dropRealtime("send failed", true);
}

void Cloud::sendAck(const char* id) {
	char esc[84], msg[260];
	if (!jsonEscape(id, esc, sizeof(esc))) return;
	snprintf(msg, sizeof(msg),
	         "{\"topic\":\"realtime:door:%s\",\"event\":\"broadcast\",\"payload\":{\"type\":\"broadcast\","
	         "\"event\":\"force_open_ack\",\"payload\":{\"id\":\"%s\"}},\"ref\":\"%lu\"}",
	         cfg_->doorId, esc, (unsigned long)++ref_);
	ws_.sendText(msg, strlen(msg));
}

void Cloud::onMessage(const char* text, size_t len) {
	static const char kFilter[] =
	    "{\"event\":true,\"ref\":true,\"payload\":{\"status\":true,\"event\":true,\"payload\":{\"id\":true,\"ts\":true},"
	    "\"data\":{\"type\":true,\"record\":{\"id\":true,\"disabled\":true,\"expires_at\":true}}}}";
	JsonDocument filter;
	deserializeJson(filter, kFilter);
	JsonDocument doc;
	if (deserializeJson(doc, text, len, DeserializationOption::Filter(filter))) {
		LOG("Unreadable realtime message; scheduling a resync.");
		syncDue_ = true;
		return;
	}
	const char* event = doc["event"] | "";

	if (!strcmp(event, "phx_reply")) {
		if (heartbeatOutstanding_ && heartbeatRef_ && atol(doc["ref"] | "0") == (long)heartbeatRef_)
			heartbeatOutstanding_ = false;
	} else if (!strcmp(event, "phx_error") || !strcmp(event, "phx_close")) {
		dropRealtime("channel error", true);
	} else if (!strcmp(event, "system")) {
		if (!strcmp(doc["payload"]["status"] | "", "error")) {
			access_[0] = 0;  // most likely an expired or revoked token
			dropRealtime("server reported an error", true);
		}
	} else if (!strcmp(event, "broadcast")) {
		if (strcmp(doc["payload"]["event"] | "", "force_open") != 0) return;
		const char* id = doc["payload"]["payload"]["id"] | "";
		double sentAt = doc["payload"]["payload"]["ts"] | 0.0;
		if (!safeId(id)) { LOG("Ignoring force_open without a valid id."); return; }

		// A command is only valid shortly after it was sent (needs a synced clock to judge).
		if (clock_->synced() && sentAt > 0) {
			double skew = (double)clock_->now() * 1000.0 - sentAt;
			if (skew > kCommandMaxAgeMs || skew < -(double)kCommandMaxAgeMs) {
				LOG("Ignoring stale force_open (%.0f s old).", skew / 1000.0);
				return;
			}
		}
		bool repeat = false;
		for (auto& seen : recentCommands_)
			if (!strcmp(seen, id)) repeat = true;
		if (!repeat) {
			strlcpy(recentCommands_[recentNext_++ % 4], id, sizeof(recentCommands_[0]));
			LOG("Force open command received.");
			if (hooks_.forceOpen) hooks_.forceOpen();
		}
		sendAck(id);  // also answers a resend whose first acknowledgement got lost
	} else if (!strcmp(event, "postgres_changes")) {
		const char* type = doc["payload"]["data"]["type"] | "";
		if (!strcmp(type, "DELETE")) return;  // a deleted key keeps its blacklist entry until it expires
		const char* id = doc["payload"]["data"]["record"]["id"] | "";
		if (!*id) { syncDue_ = true; return; }
		if (doc["payload"]["data"]["record"]["disabled"] | false) {
			uint32_t exp = parseIso(doc["payload"]["data"]["record"]["expires_at"] | "");
			blacklist_->upsert(id, exp ? exp : 0xFFFFFFFFu);
			LOG("Key disabled: %s", id);
		} else {
			blacklist_->remove(id);
			LOG("Key enabled: %s", id);
		}
		blacklistDirty_ = true;
	}
}

void Cloud::pump(uint32_t ms) {
	for (int i = 0; i < 8 && joined_; i++) {
		WsClient::Event e = ws_.poll();
		if (e == WsClient::Event::None) break;
		if (e == WsClient::Event::Message) {
			lastRxMs_ = ms;
			onMessage(ws_.text(), ws_.length());
		} else if (e == WsClient::Event::Dropped) {
			LOG("Oversized realtime message dropped; scheduling a resync.");
			syncDue_ = true;
		} else if (e == WsClient::Event::Closed) {
			dropRealtime("connection closed", true);
			return;
		}
	}
	if (!joined_) return;
	if (ms - lastHeartbeatMs_ >= kHeartbeatMs) {
		// No reply to the previous heartbeat: the TCP connection is half open (router reboot, cable
		// pulled). Reconnect rather than wait for a timeout that may never come.
		if (heartbeatOutstanding_) dropRealtime("heartbeat timed out", true);
		else sendHeartbeat();
	}
}

// ------------------------------------------------------------------------------------ control

bool Cloud::maintenanceDue(uint32_t ms) const {
	bool sync = (syncDue_ || ms - lastSyncMs_ >= kSyncEveryMs) && (int32_t)(ms - syncRetryMs_) >= 0;
	bool journal = Persist::journalCount() > 0 && (int32_t)(ms - journalRetryMs_) >= 0;
	return tokenNeedsRefresh(ms) || sync || journal;
}

void Cloud::backoff(bool permanent) {
	failures_++;
	uint32_t base = 3000UL << (failures_ > 8 ? 7 : failures_ - 1);
	if (base > 300000) base = 300000;
	if (permanent || authRejected_) base = 300000;
	uint32_t wait = base + ESP.random() % (base / 4 + 1);  // jitter keeps several doors from syncing in step
	nextAttemptMs_ = millis() + wait;
	LOG("Cloud unavailable; retrying in %lu s.", (unsigned long)(wait / 1000));
}

// One connection cycle: sign in, do the REST work that is due, then open the websocket.
void Cloud::cycle(uint32_t ms) {
	if (!ensureSession()) { backoff(authRejected_); return; }

	if (Persist::journalCount() && (int32_t)(ms - journalRetryMs_) >= 0) flushJournal();
	if ((syncDue_ || ms - lastSyncMs_ >= kSyncEveryMs) && (int32_t)(ms - syncRetryMs_) >= 0) syncBlacklist();

	if (!openRealtime()) backoff(authRejected_);
}

void Cloud::service(bool quiet) {
	uint32_t ms = millis();
	manageWifi(ms);
	if (quiet) persistBlacklistIfDirty();

	if (WiFi.status() != WL_CONNECTED) {
		if (joined_) dropRealtime("Wi-Fi lost", false);
		return;
	}
	if (joined_) {
		pump(ms);
		if (!joined_) return;
		if (!quiet || !maintenanceDue(ms)) return;
		dropRealtime("maintenance window", false);  // then reconnect below, immediately
	} else {
		if (!quiet || (int32_t)(ms - nextAttemptMs_) < 0) return;
	}
	cycle(ms);
}

void Cloud::printStatus() {
	LOG("wifi      : %s (%d dBm)", WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "down", WiFi.RSSI());
	LOG("realtime  : %s", joined_ ? "connected" : "offline");
	LOG("token     : %s", access_[0] ? "valid" : "none");
	LOG("tls       : %s buffers, %lu low-memory refusals", mflnOk_ ? "1 KB" : "16 KB", (unsigned long)lowMemory_);
	LOG("last sync : %s", lastSyncMs_ ? "done" : "never");
}
