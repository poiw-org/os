#include "Realtime.h"
#include <WiFi.h>
#include <string.h>
#include "Chimes.h"
#include "Log.h"
#include "TimeUtil.h"

namespace {
constexpr uint32_t kConnectMs = 8000;
// Heartbeat every 10 s, answer expected within 4 s: a half-open connection (router reboot, NAT
// entry lost) is found in at most 14 s instead of the 50 s a 25 s heartbeat needs.
constexpr uint32_t kHeartbeatMs = 10000;
constexpr uint32_t kHeartbeatTimeoutMs = 4000;
constexpr uint32_t kMaxWaitMs = 500;        // longest sleep, so Wi-Fi and token changes are noticed
constexpr uint32_t kWriteWaitMs = 50;      // longest sleep during a writer session
constexpr uint32_t kStableMs = 60000;       // a connection this old that drops starts the backoff over
constexpr uint32_t kRefusedHoldMs = 60000;  // a refused join will not be fixed by retrying faster
constexpr uint32_t kCommandMaxAgeMs = 120000;
constexpr size_t kSuffixReserve = 64;       // room after the token for the end of a message

constexpr uint32_t kTaskStack = 9216;       // a TLS handshake alone needs about 5 KB; `status` shows the headroom
constexpr UBaseType_t kTaskPriority = 2;    // above the REST task: a command never waits for an upload
constexpr BaseType_t kTaskCore = 0;         // with Wi-Fi and lwIP, away from the NFC reader and loop()

// Only these fields of a realtime message are parsed; the rest is skipped while reading.
const char kFilter[] =
    "{\"event\":true,\"ref\":true,\"payload\":{\"status\":true,\"message\":true,\"response\":{\"reason\":true},"
    "\"event\":true,\"payload\":{\"id\":true,\"ts\":true,\"token\":true},"
    "\"data\":{\"type\":true,\"record\":{\"id\":true,\"legacy_id\":true,\"disabled\":true,\"expires_at\":true}}}}";

// Built once and kept: parsing the filter for every message would allocate and free it each time.
const JsonDocument& messageFilter() {
	static JsonDocument filter;
	static bool built = false;
	if (!built) {
		deserializeJson(filter, kFilter);
		built = true;
	}
	return filter;
}

bool safeId(const char* id) {
	size_t n = strlen(id);
	if (n == 0 || n > 39) return false;
	for (size_t i = 0; i < n; i++)
		if (!isalnum((unsigned char)id[i]) && id[i] != '-' && id[i] != '_') return false;
	return true;
}

// Case-insensitive search for a lower-case word.
bool mentions(const char* text, const char* word) {
	size_t n = strlen(word);
	for (; *text; text++) {
		size_t i = 0;
		while (i < n && text[i] && (text[i] | 0x20) == word[i]) i++;
		if (i == n) return true;
	}
	return false;
}

// A server complaint that means the access token is no longer good (as opposed to, say, a
// subscription that cannot be created, which a new token does not fix).
bool blamesToken(const char* message) { return mentions(message, "token") || mentions(message, "jwt"); }
}  // namespace

void Realtime::begin(const Settings& settings, Clock& clock, BlacklistSet& blacklist, AccessToken& token,
                     RealtimeEvents& events) {
	cfg_ = &settings;
	clock_ = &clock;
	blacklist_ = &blacklist;
	token_ = &token;
	events_ = &events;
	xTaskCreatePinnedToCore(&Realtime::run, "rt", kTaskStack, this, kTaskPriority, &task_, kTaskCore);
}

void Realtime::run(void* self) { static_cast<Realtime*>(self)->loop(); }

void Realtime::loop() {
	bool wifiWasDown = true;
	for (;;) {
		if (WiFi.status() != WL_CONNECTED) {
			wifiWasDown = true;
			vTaskDelay(pdMS_TO_TICKS(500));
			continue;
		}
		if (wifiWasDown) {
			wifiWasDown = false;
			backoff_.succeed();  // Wi-Fi is back: connect at once instead of waiting out an old delay
		}
		if (!token_->present() || !backoff_.due(millis())) {
			vTaskDelay(pdMS_TO_TICKS(100));
			continue;
		}
		switch (connect()) {
			case Outcome::Joined: session(); break;  // returns once drop() has scheduled the retry
			case Outcome::Failed: retryLater(); break;
			case Outcome::Refused:
				backoff_.hold(millis(), kRefusedHoldMs);
				LOG("Realtime unavailable; retrying in %lu s.", (unsigned long)(kRefusedHoldMs / 1000));
				break;
		}
	}
}

void Realtime::retryLater() {
	uint32_t wait = backoff_.fail(millis(), esp_random());
	LOG("Realtime unavailable; retrying in %lu s.", (unsigned long)((wait + 999) / 1000));
}

Realtime::Outcome Realtime::connect() {
	char path[320];
	snprintf(path, sizeof(path), "/realtime/v1/websocket?apikey=%s&vsn=1.0.0", cfg_->anonKey);
	if (!ws_.open(cfg_->supabaseHost, path, kConnectMs)) return Outcome::Failed;

	// The join message is composed in the send buffer, the token copied straight into it.
	char* msg = ws_.sendBuffer();
	uint32_t ref = ++ref_;
	int n = snprintf(msg, WsClient::kMaxSend,
	                 "{\"topic\":\"realtime:door:%s\",\"event\":\"phx_join\",\"payload\":{\"config\":{"
	                 "\"broadcast\":{\"ack\":false,\"self\":false},\"presence\":{\"key\":\"\"},"
	                 "\"postgres_changes\":[{\"event\":\"*\",\"schema\":\"public\",\"table\":\"keys\"}],\"private\":true},"
	                 "\"access_token\":\"",
	                 cfg_->doorId);
	uint32_t generation = 0;
	if (n <= 0 || (size_t)n + kSuffixReserve >= WsClient::kMaxSend ||
	    !token_->copyTo(msg + n, WsClient::kMaxSend - (size_t)n - kSuffixReserve, &generation)) {
		ws_.close();
		return Outcome::Failed;
	}
	n += (int)strlen(msg + n);
	n += snprintf(msg + n, WsClient::kMaxSend - (size_t)n, "\"},\"ref\":\"%lu\",\"join_ref\":\"%lu\"}",
	              (unsigned long)ref, (unsigned long)ref);
	if (!ws_.send((size_t)n)) {
		ws_.close();
		return Outcome::Failed;
	}
	pushedGeneration_ = generation;

	char expected[12];
	snprintf(expected, sizeof(expected), "%lu", (unsigned long)ref);
	uint32_t deadline = millis() + kConnectMs;
	for (;;) {
		int32_t left = (int32_t)(deadline - millis());
		if (left <= 0) break;
		ws_.wait(left < 200 ? (uint32_t)left : 200);
		WsClient::Event e = ws_.poll();
		if (e == WsClient::Event::Closed) break;
		if (e != WsClient::Event::Message) continue;

		JsonDocument doc;
		if (deserializeJson(doc, ws_.text(), ws_.length(), DeserializationOption::Filter(messageFilter()))) continue;
		if (strcmp(doc["event"] | "", "phx_reply") != 0 || strcmp(doc["ref"] | "", expected) != 0) continue;

		if (!strcmp(doc["payload"]["status"] | "", "ok")) {
			joined_ = true;
			events_->joined.store(true, std::memory_order_release);
			events_->resync.store(true, std::memory_order_release);  // whatever happened while we were away
			heartbeatOutstanding_ = false;
			lastHeartbeatMs_ = joinedAtMs_ = millis();
			LOG("Realtime connected (channel door:%s).", cfg_->doorId);
			return Outcome::Joined;
		}
		const char* reason = doc["payload"]["response"]["reason"] | "no reason given";
		LOG("Realtime join refused: %s. Is the door account listed in public.door_users?", reason);
		ws_.close();
		if (blamesToken(reason)) {
			// Stale token (typically after an outage): the REST task signs in again and the loop
			// reconnects as soon as the new token arrives, so no long hold.
			token_->clear();
			return Outcome::Failed;
		}
		return Outcome::Refused;
	}
	ws_.close();
	return Outcome::Failed;
}

void Realtime::drop(const char* why, bool failure) {
	LOG("Realtime closed: %s.", why);
	ws_.close();
	joined_ = false;
	events_->joined.store(false, std::memory_order_release);
	heartbeatOutstanding_ = false;
	if (!failure) return;
	if (millis() - joinedAtMs_ >= kStableMs) {
		backoff_.succeed();  // it had been fine for a while: reconnect at once
		LOG("Realtime reconnecting.");
		return;
	}
	retryLater();
}

void Realtime::session() {
	while (joined_) {
		if (WiFi.status() != WL_CONNECTED) {
			drop("Wi-Fi lost", false);
			return;
		}
		uint32_t deadline = lastHeartbeatMs_ + (heartbeatOutstanding_ ? kHeartbeatTimeoutMs : kHeartbeatMs);
		int32_t left = (int32_t)(deadline - millis());
		// While a writer session runs, wake often enough that its state reaches the screen promptly.
		bool prompt = writeActive(events_->writeState.load(std::memory_order_acquire)) ||
		              events_->btAsking.load(std::memory_order_acquire);
		uint32_t cap = prompt ? kWriteWaitMs : kMaxWaitMs;
		if (left > 0) ws_.wait((uint32_t)left < cap ? (uint32_t)left : cap);

		drain();
		if (!joined_) return;
		reportWrite();
		if (!joined_) return;
		reportBluetooth();
		if (!joined_) return;

		uint32_t now = millis();
		if (heartbeatOutstanding_) {
			// No answer: the connection is half open. Reconnect instead of waiting for a TCP timeout.
			if ((int32_t)(now - (lastHeartbeatMs_ + kHeartbeatTimeoutMs)) >= 0) {
				drop("heartbeat timed out", true);
				return;
			}
		} else if ((int32_t)(now - (lastHeartbeatMs_ + kHeartbeatMs)) >= 0) {
			sendHeartbeat();
		}
		if (joined_ && token_->present() && token_->generation() != pushedGeneration_) pushToken();
	}
}

void Realtime::drain() {
	for (int i = 0; i < 16 && joined_; i++) {
		switch (ws_.poll()) {
			case WsClient::Event::None: return;
			case WsClient::Event::Message: onMessage(ws_.text(), ws_.length()); break;
			case WsClient::Event::Dropped:
				LOG("Oversized realtime message skipped; scheduling a resync.");
				events_->resync.store(true, std::memory_order_release);
				break;
			case WsClient::Event::Closed:
				drop("connection closed", true);
				return;
		}
	}
}

// Gives the server the renewed access token on the open channel. Without it the server ends the
// channel when the old token expires.
bool Realtime::pushToken() {
	char* msg = ws_.sendBuffer();
	int n = snprintf(msg, WsClient::kMaxSend,
	                 "{\"topic\":\"realtime:door:%s\",\"event\":\"access_token\",\"payload\":{\"access_token\":\"",
	                 cfg_->doorId);
	uint32_t generation = 0;
	if (n <= 0 || (size_t)n + kSuffixReserve >= WsClient::kMaxSend ||
	    !token_->copyTo(msg + n, WsClient::kMaxSend - (size_t)n - kSuffixReserve, &generation))
		return false;
	n += (int)strlen(msg + n);
	n += snprintf(msg + n, WsClient::kMaxSend - (size_t)n, "\"},\"ref\":\"%lu\"}", (unsigned long)++ref_);
	if (!ws_.send((size_t)n)) {
		drop("send failed", true);
		return false;
	}
	pushedGeneration_ = generation;
	LOG("Realtime access token renewed.");
	return true;
}

void Realtime::sendHeartbeat() {
	heartbeatRef_ = ++ref_;
	char* msg = ws_.sendBuffer();
	int n = snprintf(msg, WsClient::kMaxSend, "{\"topic\":\"phoenix\",\"event\":\"heartbeat\",\"payload\":{},\"ref\":\"%lu\"}",
	                 (unsigned long)heartbeatRef_);
	heartbeatOutstanding_ = true;
	lastHeartbeatMs_ = millis();
	if (n <= 0 || !ws_.send((size_t)n)) drop("send failed", true);
}

void Realtime::sendAck(const char* id) {
	char* msg = ws_.sendBuffer();
	int n = snprintf(msg, WsClient::kMaxSend,
	                 "{\"topic\":\"realtime:door:%s\",\"event\":\"broadcast\",\"payload\":{\"type\":\"broadcast\","
	                 "\"event\":\"force_open_ack\",\"payload\":{\"id\":\"%s\",\"delay_ms\":%lu}},\"ref\":\"%lu\"}",
	                 cfg_->doorId, id,  // id passed safeId(): no escaping needed
	                 (unsigned long)Chimes::kForceOpenDelayMs, (unsigned long)++ref_);
	if (n <= 0 || !ws_.send((size_t)n)) drop("send failed", true);
}

// Broadcasts are not stored, so a request is repeated until it ends: an admin who opens the webapp
// during the window still sees it, and the time left keeps every admin's countdown honest.
void Realtime::reportBluetooth() {
	static constexpr uint32_t kResendMs = 2000;
	BtOutcome outcome = events_->btOutcome.exchange(BtOutcome::None, std::memory_order_acq_rel);
	if (outcome != BtOutcome::None) {
		static const char* const kNames[] = {"", "approved", "denied", "expired", "gone"};
		char* msg = ws_.sendBuffer();
		int n = snprintf(msg, WsClient::kMaxSend,
		                 "{\"topic\":\"realtime:door:%s\",\"event\":\"broadcast\",\"payload\":{\"type\":\"broadcast\","
		                 "\"event\":\"bt_result\",\"payload\":{\"id\":\"%s\",\"outcome\":\"%s\"}},\"ref\":\"%lu\"}",
		                 cfg_->doorId, events_->btDoneId, kNames[(int)outcome], (unsigned long)++ref_);
		if (n <= 0 || !ws_.send((size_t)n)) {
			drop("send failed", true);
			return;
		}
	}
	if (!events_->btAsking.load(std::memory_order_acquire)) return;
	uint32_t now = millis();
	if (!strcmp(btSentId_, events_->btId) && now - btSentMs_ < kResendMs) return;
	strlcpy(btSentId_, events_->btId, sizeof(btSentId_));
	btSentMs_ = now;
	int32_t left = (int32_t)(events_->btDeadlineMs.load(std::memory_order_acquire) - now);
	char* msg = ws_.sendBuffer();
	int n = snprintf(msg, WsClient::kMaxSend,
	                 "{\"topic\":\"realtime:door:%s\",\"event\":\"broadcast\",\"payload\":{\"type\":\"broadcast\","
	                 "\"event\":\"bt_request\",\"payload\":{\"id\":\"%s\",\"address\":\"%s\",\"left_ms\":%ld}},\"ref\":\"%lu\"}",
	                 cfg_->doorId, btSentId_, events_->btAddress, (long)(left > 0 ? left : 0), (unsigned long)++ref_);
	if (n <= 0 || !ws_.send((size_t)n)) drop("send failed", true);
}

// A command is only valid with a safe id and shortly after it was sent (judged with a synced clock).
bool Realtime::commandValid(const char* what, const char* id, double sentAt) {
	if (!safeId(id)) {
		LOG("Ignoring %s without a valid id.", what);
		return false;
	}
	if (clock_->synced() && sentAt > 0) {
		double skew = (double)clock_->now() * 1000.0 - sentAt;
		if (skew > kCommandMaxAgeMs || skew < -(double)kCommandMaxAgeMs) {
			LOG("Ignoring stale %s (%.0f s off; is the sender's clock right?).", what, skew / 1000.0);
			return false;
		}
	}
	return true;
}

// True the first time a command id is seen; a resend (the sender repeats until it hears back) is not.
bool Realtime::firstSight(const char* id) {
	for (const auto& seen : recentCommands_)
		if (!strcmp(seen, id)) return false;
	strlcpy(recentCommands_[recentNext_++ % 4], id, sizeof(recentCommands_[0]));
	return true;
}

void Realtime::sendWriteStatus(const char* id, WriteState state) {
	char* msg = ws_.sendBuffer();
	int n = snprintf(msg, WsClient::kMaxSend,
	                 "{\"topic\":\"realtime:door:%s\",\"event\":\"broadcast\",\"payload\":{\"type\":\"broadcast\","
	                 "\"event\":\"write_card_status\",\"payload\":{\"id\":\"%s\",\"state\":\"%s\",\"timeout_ms\":%lu,\"serial\":\"%s\"}},"
	                 "\"ref\":\"%lu\"}",
	                 cfg_->doorId, id, writeStateName(state), (unsigned long)kWriteWindowMs,
	                 state == WriteState::Done ? events_->scanSerial : "", (unsigned long)++ref_);
	if (n <= 0 || !ws_.send((size_t)n)) drop("send failed", true);
}

// Tells the backoffice when loop() moved the writer session on.
void Realtime::reportWrite() {
	uint32_t seq = events_->writeStateSeq.load(std::memory_order_acquire);
	if (seq == writeSentSeq_) return;
	writeSentSeq_ = seq;
	WriteState s = events_->writeState.load(std::memory_order_acquire);
	if (s != WriteState::Idle && writeId_[0]) sendWriteStatus(writeId_, s);
}

void Realtime::onWriteCard(const JsonDocument& doc, const char* id, double sentAt, bool scan) {
	if (!commandValid(scan ? "scan_card" : "write_card", id, sentAt)) return;
	if (!firstSight(id)) {
		WriteState s = events_->writeState.load(std::memory_order_acquire);
		if (!strcmp(id, writeId_) && s != WriteState::Idle) sendWriteStatus(id, s);  // the first answer got lost
		return;
	}
	const char* token = doc["payload"]["payload"]["token"] | "";
	size_t n = strlen(token);
	if ((n == 0 && !scan) || n > kMaxTokenLen || events_->writePending.load(std::memory_order_acquire)) {
		LOG("Ignoring write_card: %s.", n ? (n > kMaxTokenLen ? "token too long" : "another is pending") : "no token");
		sendWriteStatus(id, WriteState::Rejected);
		return;
	}
	strlcpy(writeId_, id, sizeof(writeId_));
	strlcpy(events_->write.id, id, sizeof(events_->write.id));
	memcpy(events_->write.token, token, n + 1);  // empty for a scan
	events_->scanSerial[0] = 0;
	events_->writeState.store(WriteState::Idle, std::memory_order_release);  // nothing to report until loop() answers
	writeSentSeq_ = events_->writeStateSeq.load(std::memory_order_acquire);
	events_->writePending.store(true, std::memory_order_release);
	commands_++;
	LOG("Writer mode requested.");
}

void Realtime::onBroadcast(const JsonDocument& doc) {
	const char* event = doc["payload"]["event"] | "";
	const char* id = doc["payload"]["payload"]["id"] | "";
	double sentAt = doc["payload"]["payload"]["ts"] | 0.0;

	if (!strcmp(event, "write_card")) return onWriteCard(doc, id, sentAt, false);
	if (!strcmp(event, "scan_card")) return onWriteCard(doc, id, sentAt, true);
	if (!strcmp(event, "write_cancel")) {
		if (safeId(id) && !strcmp(id, writeId_)) events_->writeCancel.store(true, std::memory_order_release);
		return;
	}
	if (!strcmp(event, "bt_approve") || !strcmp(event, "bt_deny")) {
		if (commandValid(event, id, sentAt) && events_->btAsking.load(std::memory_order_acquire) && !strcmp(id, events_->btId))
			events_->btDecision.store(event[3] == 'a' ? BtDecision::Approve : BtDecision::Deny, std::memory_order_release);
		return;
	}
	if (strcmp(event, "force_open") != 0 || !commandValid("force_open", id, sentAt)) return;
	if (firstSight(id)) {
		events_->forceOpen.store(true, std::memory_order_release);  // loop() opens the door within a millisecond
		commands_++;
		LOG("Force open command received; unlocking in %lu s.", (unsigned long)(Chimes::kForceOpenDelayMs / 1000));
	}
	sendAck(id);  // also answers a resend whose first acknowledgement got lost
}

void Realtime::onKeyChange(const JsonDocument& doc) {
	JsonObjectConst data = doc["payload"]["data"];
	// A DELETE is not applied here, but the next full sync (door_sync) only lists keys that still
	// exist, so a deleted disabled key leaves the blacklist then. Keeping it blocked until its
	// expiry is the backend's job (a tombstone in door_sync, or refusing the delete).
	if (!strcmp(data["type"] | "", "DELETE")) return;
	JsonObjectConst record = data["record"];
	const char* id = record["id"] | "";
	if (!*id) {
		events_->resync.store(true, std::memory_order_release);
		return;
	}
	// A v1 key's card may still carry its Firestore id as jti, so the key is listed under both ids.
	const char* legacyId = record["legacy_id"] | "";
	if (record["disabled"] | false) {
		uint32_t exp = parseIso(record["expires_at"] | "");
		uint32_t until = exp ? exp : 0xFFFFFFFFu;  // no readable expiry: block until it is resynced
		blacklist_->upsert(id, until);
		if (*legacyId) blacklist_->upsert(legacyId, until);
		LOG("Key disabled: %s%s%s", id, *legacyId ? " / " : "", legacyId);
	} else {
		blacklist_->remove(id);
		if (*legacyId) blacklist_->remove(legacyId);
		LOG("Key enabled: %s%s%s", id, *legacyId ? " / " : "", legacyId);
	}
	events_->blacklistChanged.store(true, std::memory_order_release);
}

void Realtime::onMessage(const char* text, size_t len) {
	JsonDocument doc;
	if (deserializeJson(doc, text, len, DeserializationOption::Filter(messageFilter()))) {
		LOG("Unreadable realtime message; scheduling a resync.");
		events_->resync.store(true, std::memory_order_release);
		return;
	}
	const char* event = doc["event"] | "";

	if (!strcmp(event, "broadcast")) {
		onBroadcast(doc);
	} else if (!strcmp(event, "postgres_changes")) {
		onKeyChange(doc);
	} else if (!strcmp(event, "phx_reply")) {
		if (heartbeatOutstanding_ && heartbeatRef_ && (uint32_t)atol(doc["ref"] | "0") == heartbeatRef_)
			heartbeatOutstanding_ = false;
	} else if (!strcmp(event, "phx_error") || !strcmp(event, "phx_close")) {
		drop("channel error", true);
	} else if (!strcmp(event, "system")) {
		if (!strcmp(doc["payload"]["status"] | "", "error")) {
			const char* message = doc["payload"]["message"] | "no message";
			LOG("Realtime reported an error: %s", message);
			if (blamesToken(message)) token_->clear();  // expired or revoked: the REST task signs in again
			drop("server reported an error", true);
		}
	}
}

void Realtime::printStatus() const {
	LOG("realtime  : %s, %lu force opens received, task stack headroom %u of %u bytes",
	    events_ && events_->joined.load() ? "connected" : "offline", (unsigned long)commands_,
	    (unsigned)uxTaskGetStackHighWaterMark(task_), (unsigned)kTaskStack);
}
