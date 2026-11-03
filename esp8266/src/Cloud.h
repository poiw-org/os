// The door's connection to Supabase.
//
// While healthy the door holds one realtime websocket (a Phoenix channel, `door:<id>`) on which it
// receives key changes (postgres_changes on public.keys) and force-open commands (broadcast).
// An ESP8266 has room for one TLS session, so REST work (token refresh, uploading the event
// journal, resyncing the blacklist) happens in short maintenance windows when no one is using the
// door: the websocket is closed, the REST calls run, and the websocket is reopened. A tap never
// waits for the network; all access decisions are local.
//
// Every failure ends in a backoff and a retry, and the door keeps working from its local state.
#pragma once
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include "BlacklistSet.h"
#include "Clock.h"
#include "Settings.h"
#include "WsClient.h"

class Cloud {
 public:
	struct Hooks {
		void (*forceOpen)();  // open the door now
	};

	Cloud() : ws_(tls_) {}
	void begin(const Settings& settings, Clock& clock, BlacklistSet& blacklist, Hooks hooks);
	// `quiet`: the door has been idle, so a maintenance window is acceptable.
	void service(bool quiet);
	bool online() const { return joined_; }
	void printStatus();

 private:
	// Wi-Fi
	void manageWifi(uint32_t ms);
	// Session
	bool ensureSession();
	bool grantToken(bool useRefresh);
	bool tokenNeedsRefresh(uint32_t ms) const;
	// REST
	// Sends a request and reads the status line and headers. Returns the HTTP status (-1 on a
	// transport error) with the connection left open at the start of the body; call finish() when
	// done reading it.
	int request(const char* method, const char* path, const char* body, size_t bodyLen, const char* prefer,
	            bool authed);
	void finish() { tls_.stop(); }
	bool prepareTls();
	void cycle(uint32_t ms);
	void noteStatus(int code);
	bool syncBlacklist();
	bool flushJournal();
	// Realtime
	bool openRealtime();
	void dropRealtime(const char* why, bool failure);
	void pump(uint32_t ms);
	void onMessage(const char* text, size_t len);
	void sendHeartbeat();
	void sendAck(const char* id);
	bool maintenanceDue(uint32_t ms) const;
	void backoff(bool permanent);
	void persistBlacklistIfDirty();

	const Settings* cfg_ = nullptr;
	Clock* clock_ = nullptr;
	BlacklistSet* blacklist_ = nullptr;
	Hooks hooks_ = {nullptr};

	BearSSL::WiFiClientSecure tls_;
	BearSSL::X509List anchors_;
	BearSSL::Session session_;
	WsClient ws_;
	bool mflnTried_ = false, mflnOk_ = false;

	char access_[1400] = {0};
	char refresh_[160] = {0};
	uint32_t refreshAtMs_ = 0;  // when to renew the access token (millis)
	bool authRejected_ = false;

	bool joined_ = false;
	bool wifiWasUp_ = false;
	uint32_t wifiKickMs_ = 0, wifiBeginMs_ = 0;
	uint32_t nextAttemptMs_ = 0, lastRxMs_ = 0, lastHeartbeatMs_ = 0, lastSyncMs_ = 0;
	uint32_t syncRetryMs_ = 0, journalRetryMs_ = 0;
	uint32_t failures_ = 0, ref_ = 0, heartbeatRef_ = 0;
	bool heartbeatOutstanding_ = false;
	bool syncDue_ = true, blacklistDirty_ = false;
	uint32_t lowMemory_ = 0;
	char recentCommands_[4][40] = {{0}};
	uint8_t recentNext_ = 0;
};
