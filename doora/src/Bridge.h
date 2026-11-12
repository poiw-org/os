// The door's end of the UART link to the BLE bridge (docs/ble-protocol.md, section 6). The bridge is
// untrusted plumbing: this module frames bytes, keeps the bridge's liveness, issues a challenge when a
// phone subscribes and hands one request at a time to loop() through take()/release(), the way Reader
// hands over a tag. Everything about a request is judged by the caller; nothing here decides access.
// service() is called from loop() and never blocks.
#pragma once
#include <Arduino.h>
#include "BleSession.h"
#include "BridgeLink.h"

struct BleRequestSlot {
	uint8_t conn;
	const uint8_t* body;  // inside the decoder; valid until release()
	size_t len;
};

class Bridge {
 public:
	void begin(const char* doorId);
	void service(uint32_t ms);

	// A request on the live connection, or nullptr. Answer it with reply(), then release().
	const BleRequestSlot* take() const { return pending_ ? &slot_ : nullptr; }
	void release() { pending_ = false; }

	// Answers the live request: sends RESULT and spends the nonce. `opened` starts no hold.
	void reply(BleResult code, uint32_t ms);

	// The live connection's nonce, for the proof check. Valid while a request is pending.
	const uint8_t* nonce() const { return session_.nonce(); }

	bool online() const { return online_; }
	void countOpened() { opened_++; }
	void countDenied() { denied_++; }
	void printStatus() const;

 private:
	void handle(const Link::Frame& f, uint32_t ms);
	void send(uint8_t type, uint8_t conn, const uint8_t* payload = nullptr, size_t len = 0);
	void resetPulse(uint32_t ms);

	const char* doorId_ = "";
	Link::Decoder decoder_;
	BleSession session_;
	BleRequestSlot slot_ = {};
	bool pending_ = false;

	bool seen_ = false;        // a valid HELLO came from a bridge we understand
	bool online_ = false;
	bool incompatible_ = false;
	char version_[33] = "";
	uint8_t linkVersion_ = 0;
	uint32_t lastHeardMs_ = 0;
	uint32_t lastPingMs_ = 0;
	uint8_t missed_ = 0;        // pings sent since the last PONG
	uint32_t lastResetMs_ = 0;
	uint32_t resetReleaseMs_ = 0;
	bool resetting_ = false;
	uint32_t opened_ = 0, denied_ = 0;
};
