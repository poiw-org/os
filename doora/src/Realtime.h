// The door's realtime channel (a Phoenix channel `door:<id>` on Supabase). It carries force-open
// commands (broadcast) and key changes (postgres_changes on public.keys). Importers: Cloud.
//
// A task of its own owns the websocket and does nothing else, so a command never waits behind a
// REST call or a flash write: the task sleeps in select() until a frame arrives, handles it in
// well under a millisecond and tells the rest of the firmware through RealtimeEvents. The door's
// access token is renewed on the open connection, so the channel is not torn down for upkeep
// unless memory is too short for the REST task's own session (RealtimeEvents::yield).
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <atomic>
#include "AccessToken.h"
#include "Backoff.h"
#include "BlacklistSet.h"
#include "Clock.h"
#include "Jwt.h"
#include "Settings.h"
#include "WriteState.h"
#include "WsClient.h"

enum class BtDecision : uint8_t { None, Approve, Deny };
enum class BtOutcome : uint8_t { None, Approved, Denied, Expired, Gone };

// What the realtime task reports. Plain atomics: no task ever waits for another.
struct RealtimeEvents {
	std::atomic<bool> joined{false};            // the channel is up
	std::atomic<bool> forceOpen{false};         // open the door now (loop() takes it)
	std::atomic<bool> resync{true};             // the blacklist may be stale; the REST task refetches it
	std::atomic<bool> blacklistChanged{false};  // the in-memory blacklist should be written to flash
	std::atomic<bool> firmwareCheck{true};      // an update may be approved; the REST task asks
	// Memory is too short for two TLS sessions: the REST task asks for the websocket's, and the
	// realtime task closes it until the flag clears. Never set while there is room for both.
	std::atomic<bool> yield{false};
	std::atomic<uint32_t> yields{0};

	// Writer mode. The realtime task fills `write` and sets writePending; loop() consumes it and then
	// clears the flag. loop() publishes the session's state, which the realtime task reports.
	struct WriteRequest {
		char id[40];
		char token[kMaxTokenLen + 1];
	} write;
	char scanSerial[32] = {0};  // the UID a finished scan saw (loop() sets it before publishing Done)
	std::atomic<bool> writePending{false};
	std::atomic<bool> writeCancel{false};
	std::atomic<WriteState> writeState{WriteState::Idle};
	std::atomic<uint32_t> writeStateSeq{0};

	// Bluetooth approval. loop() fills the request and sets btAsking; the realtime task announces it
	// and hands back an admin's answer for that id. An approval only ever matches the current request.
	char btId[17] = {0};
	char btAddress[18] = {0};
	char btDoneId[17] = {0};
	char consoleCommand[12] = {0};  // a portal command; loop() runs it, then clears consolePending
	std::atomic<bool> consolePending{false};
	std::atomic<uint32_t> btDeadlineMs{0};
	std::atomic<bool> btAsking{false};
	std::atomic<BtDecision> btDecision{BtDecision::None};
	std::atomic<BtOutcome> btOutcome{BtOutcome::None};
};

class Realtime {
 public:
	void begin(const Settings& settings, Clock& clock, BlacklistSet& blacklist, AccessToken& token,
	           RealtimeEvents& events);
	void printStatus() const;

 private:
	enum class Outcome : uint8_t { Joined, Failed, Refused };

	static void run(void* self);
	[[noreturn]] void loop();
	Outcome connect();
	void session();
	void drain();
	void drop(const char* why, bool failure);
	void retryLater();
	void onMessage(const char* text, size_t len);
	void onBroadcast(const JsonDocument& doc);
	void onKeyChange(const JsonDocument& doc);
	bool pushToken();
	void sendHeartbeat();
	void sendAck(const char* id);
	void sendWriteStatus(const char* id, WriteState state);
	void reportWrite();
	void reportBluetooth();
	void reportConsole();
	void sendStatus();
	void onConsoleCommand(const JsonDocument& doc);
	bool commandValid(const char* what, const char* id, double sentAt);
	bool firstSight(const char* id);
	void onWriteCard(const JsonDocument& doc, const char* id, double sentAt, bool scan);

	const Settings* cfg_ = nullptr;
	Clock* clock_ = nullptr;
	BlacklistSet* blacklist_ = nullptr;
	AccessToken* token_ = nullptr;
	RealtimeEvents* events_ = nullptr;

	WsClient ws_;
	Backoff backoff_{1000, 30000};
	TaskHandle_t task_ = nullptr;

	bool joined_ = false;  // the task's own view of events_->joined
	bool yielded_ = false;  // the last session ended to lend its memory; the REST task resynced meanwhile
	bool heartbeatOutstanding_ = false;
	uint32_t ref_ = 0, heartbeatRef_ = 0;
	uint32_t lastHeartbeatMs_ = 0, joinedAtMs_ = 0;
	uint32_t pushedGeneration_ = 0;  // generation of the access token the server has
	uint32_t commands_ = 0;
	char recentCommands_[4][40] = {{0}};
	uint8_t recentNext_ = 0;
	char writeId_[40] = {0};  // the writer-mode session being reported
	uint32_t writeSentSeq_ = 0;
	char btSentId_[17] = {0};  // the Bluetooth request last announced
	uint32_t btSentMs_ = 0;
};
