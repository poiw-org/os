// The door's side of one BLE attempt: which connection is live, its nonce, and the rules that make a
// nonce good for one request only (docs/ble-protocol.md, section 4). Free of I/O and of the clock so
// every transition is unit tested; callers pass `now` in milliseconds.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "BleProto.h"

class BleSession {
 public:
	static constexpr uint32_t kNonceLifeMs = 10000;
	static constexpr uint32_t kHoldMs = 2000;  // after a failed attempt every request gets "busy"

	// The phone on connection `conn` has subscribed. Takes a fresh nonce from the caller (the door's
	// hardware RNG) and drops any earlier connection's nonce: the door serves one connection.
	void begin(uint8_t conn, const uint8_t nonce[kBleNonceLen], uint32_t now) {
		conn_ = conn;
		memcpy(nonce_, nonce, kBleNonceLen);
		issuedAt_ = now;
		live_ = true;
	}

	// The bridge reports the phone is gone.
	void gone(uint8_t conn) { if (conn == conn_) end(); }

	enum class Admit : uint8_t {
		Ignore,  // not the live connection, or it already has an answer: say nothing
		Busy,    // the live connection, but its nonce is stale or the door is holding off: answer "busy"
		Proceed  // check the request against nonce()
	};
	Admit admit(uint8_t conn, uint32_t now) const {
		if (!live_ || conn != conn_) return Admit::Ignore;
		if (now - issuedAt_ >= kNonceLifeMs) return Admit::Busy;
		if (holding_ && (int32_t)(holdUntil_ - now) > 0) return Admit::Busy;
		return Admit::Proceed;
	}

	// The request was answered, whatever the answer: the nonce is spent. A failure starts the hold.
	void answered(bool opened, uint32_t now) {
		end();
		if (!opened) { holding_ = true; holdUntil_ = now + kHoldMs; }
	}

	bool live() const { return live_; }
	uint8_t conn() const { return conn_; }
	const uint8_t* nonce() const { return nonce_; }

 private:
	void end() {
		live_ = false;
		memset(nonce_, 0, sizeof(nonce_));
	}
	uint8_t conn_ = 0;
	uint8_t nonce_[kBleNonceLen] = {};
	uint32_t issuedAt_ = 0;
	uint32_t holdUntil_ = 0;
	bool live_ = false;
	bool holding_ = false;
};
