// The door's connection to Supabase, in three parts that never wait for each other:
//
//  - the realtime task (Realtime.h) holds a websocket on the private channel `door:<id>`, which
//    carries force-open commands and live key changes;
//  - the REST task (here) signs in, uploads the event journal and refetches the blacklist over
//    short TLS sessions of its own, while the websocket stays up;
//  - loop() only supervises Wi-Fi and collects what the other two report.
//
// So a tap never waits for the network, and a force-open command never waits for an upload, a
// token refresh or a flash write. All access decisions are local; every failure ends in a backoff
// and a retry, and the door keeps working from its local state.
//
// Two TLS sessions are open at the same time while both tasks work. That costs about 80 KB of
// heap. When memory does not allow the second one, the REST task borrows the websocket's: the
// realtime task closes it for that pass (RealtimeEvents::yield) and reconnects afterwards.
#pragma once
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "AccessToken.h"
#include "Backoff.h"
#include "BlacklistSet.h"
#include "Clock.h"
#include "Realtime.h"
#include "Settings.h"
#include "SigningKey.h"

class Cloud {
 public:
	void begin(const Settings& settings, Clock& clock, BlacklistSet& blacklist, SigningKey& signingKey);
	// Wi-Fi supervision; cheap, call it every loop.
	void service();
	bool online() const { return events_.joined.load(std::memory_order_acquire); }
	// True once per force-open command received since the last call.
	bool takeForceOpen() { return events_.forceOpen.exchange(false, std::memory_order_acq_rel); }
	// Writer mode. A request from the backoffice stays readable until releaseWriteRequest().
	const RealtimeEvents::WriteRequest* writeRequest() const {
		return events_.writePending.load(std::memory_order_acquire) ? &events_.write : nullptr;
	}
	void releaseWriteRequest() { events_.writePending.store(false, std::memory_order_release); }
	void setScanSerial(const char* serial) { strlcpy(events_.scanSerial, serial, sizeof(events_.scanSerial)); }
	bool takeWriteCancel() { return events_.writeCancel.exchange(false, std::memory_order_acq_rel); }
	// Publishes where the writer session stands; the realtime task reports it to the backoffice.
	void reportWrite(WriteState s) {
		events_.writeState.store(s, std::memory_order_release);
		events_.writeStateSeq.fetch_add(1, std::memory_order_acq_rel);
	}
	// A portal command, readable until released.
	const char* remoteCommand() const {
		return events_.consolePending.load(std::memory_order_acquire) ? events_.consoleCommand : nullptr;
	}
	void releaseRemoteCommand() { events_.consolePending.store(false, std::memory_order_release); }
	// Bluetooth approval: asks the admins (the realtime task repeats it until it ends) and reports how it ended.
	void requestBluetooth(const char* id, const char* address, uint32_t deadlineMs) {
		strlcpy(events_.btId, id, sizeof(events_.btId));
		strlcpy(events_.btAddress, address, sizeof(events_.btAddress));
		events_.btDeadlineMs.store(deadlineMs, std::memory_order_release);
		events_.btDecision.store(BtDecision::None, std::memory_order_release);
		events_.btAsking.store(true, std::memory_order_release);
	}
	BtDecision takeBluetoothDecision() {
		return events_.btDecision.exchange(BtDecision::None, std::memory_order_acq_rel);
	}
	void endBluetoothRequest(BtOutcome outcome) {
		strlcpy(events_.btDoneId, events_.btId, sizeof(events_.btDoneId));
		events_.btAsking.store(false, std::memory_order_release);
		events_.btOutcome.store(outcome, std::memory_order_release);
	}
	void printStatus();

 private:
	static constexpr size_t kFlushBatch = 8;
	static constexpr size_t kRecordJson = 640;

	static void runRest(void* self);
	[[noreturn]] void restLoop();
	void manageWifi(uint32_t ms);
	// Session
	void refreshSession(uint32_t ms);
	bool grantToken(bool useRefresh);
	// REST
	// Sends a request and reads the status line and headers. Returns the HTTP status (-1 on a
	// transport error) with the connection left open at the start of the body; call finish() when
	// done reading it.
	int request(const char* method, const char* path, const char* body, size_t bodyLen, const char* prefer,
	            bool authed);
	void finish() { tls_.stop(); }
	void noteStatus(int code);
	bool syncDue(uint32_t ms) const;
	bool syncBlacklist();
	bool fetchSigningKey();
	bool flushJournal();
	void updateFirmware();
	bool downloadFirmware(const char* sha256, size_t size);
	bool reportFirmware(const char* id, const char* state, const char* error);
	void reportFirmwareOutcome();
	void persistBlacklistIfDirty(uint32_t ms);

	const Settings* cfg_ = nullptr;
	Clock* clock_ = nullptr;
	BlacklistSet* blacklist_ = nullptr;
	SigningKey* signingKey_ = nullptr;

	// Shared with the realtime task.
	AccessToken token_;
	RealtimeEvents events_;
	Realtime realtime_;

	// Owned by the REST task.
	WiFiClientSecure tls_;
	TaskHandle_t restTask_ = nullptr;
	char refresh_[160] = {0};
	uint32_t refreshAtMs_ = 0;  // when to renew the access token (millis)
	bool authRejected_ = false;
	Backoff authBackoff_{3000, 300000};
	Backoff syncBackoff_{3000, 300000};
	Backoff journalBackoff_{3000, 300000};
	Backoff keyBackoff_{3000, 300000};
	uint32_t keyFetchedMs_ = 0;
	bool keyFetched_ = false;
	bool everSynced_ = false;
	uint32_t lastSyncMs_ = 0;
	bool urgent_ = false;     // the last sync was overtaken by a realtime change; sync again at once
	uint8_t supersededRuns_ = 0;  // syncs in a row overtaken by a realtime change
	bool truncated_ = false;  // the last sync had more disabled keys than the blacklist holds
	bool dirtySeen_ = false;
	uint32_t dirtySeenMs_ = 0;
	BlacklistSet staging_;                                   // a download is built here, then swapped in
	char header_[2048];                                      // one request's headers
	char body_[kFlushBatch * kRecordJson + 20];              // one batch of events as JSON

	// Wi-Fi supervision (loop task).
	bool wifiWasUp_ = false;
	uint32_t wifiKickMs_ = 0, wifiBeginMs_ = 0;
};
