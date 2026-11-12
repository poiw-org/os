#include "Bridge.h"
#include <esp_system.h>
#include "BleProto.h"
#include "Log.h"
#include "Pins.h"

namespace {
constexpr uint32_t kPingEveryMs = 5000;
constexpr uint8_t kMissedPongs = 3;             // then the bridge counts as offline
constexpr uint32_t kSilentBeforeResetMs = 30000;
constexpr uint32_t kResetGapMs = 60000;         // at most one reset a minute
constexpr uint32_t kResetLowMs = 100;
}  // namespace

void Bridge::begin(const char* doorId) {
	doorId_ = doorId;
	Serial2.setRxBufferSize(2048);
	Serial2.begin(115200, SERIAL_8N1, BRIDGE_RX, BRIDGE_TX);
	lastHeardMs_ = millis();
}

void Bridge::send(uint8_t type, uint8_t conn, const uint8_t* payload, size_t len) {
	uint8_t out[Link::kMaxEncoded];
	size_t n = Link::encode(type, conn, payload, len, out, sizeof(out));
	// Whatever does not fit the UART's buffer now is dropped rather than waited for: loop() must not
	// block, and the phone simply retries.
	if (n && (size_t)Serial2.availableForWrite() >= n) Serial2.write(out, n);
}

void Bridge::handle(const Link::Frame& f, uint32_t ms) {
	lastHeardMs_ = ms;
	if (f.type == Link::kHello) {
		linkVersion_ = f.len ? f.payload[0] : 0;
		incompatible_ = linkVersion_ != Link::kVersion;
		size_t v = f.len > 1 ? f.len - 1 : 0;
		if (v > sizeof(version_) - 1) v = sizeof(version_) - 1;
		memset(version_, 0, sizeof(version_));
		for (size_t i = 0; i < v; i++) version_[i] = (f.payload[1 + i] >= 0x20 && f.payload[1 + i] < 0x7F) ? (char)f.payload[1 + i] : '?';
		seen_ = true;
		if (incompatible_) LOG("Bridge speaks link version %u, this door %u; ignoring it.", linkVersion_, Link::kVersion);
		else LOG("Bridge online, firmware %s.", version_);
		return;
	}
	if (!seen_ || incompatible_) return;
	switch (f.type) {
		case Link::kPong:
			missed_ = 0;
			online_ = true;
			break;
		case Link::kConnected: {
			// The nonce is made now, on the door, and goes to the bridge only inside the challenge.
			uint8_t nonce[kBleNonceLen];
			esp_fill_random(nonce, sizeof(nonce));
			uint8_t msg[3 + kBleNonceLen + kBleMaxDoorId];
			size_t n = bleEncodeChallenge(nonce, doorId_, msg, sizeof(msg));
			if (!n) { LOG("Door id \"%s\" does not fit a BLE challenge (1-31 bytes).", doorId_); break; }
			session_.begin(f.conn, nonce, ms);
			send(Link::kChallenge, f.conn, msg, n);
			break;
		}
		case Link::kGone:
			session_.gone(f.conn);
			break;
		case Link::kRequest: {
			BleSession::Admit a = session_.admit(f.conn, ms);
			if (a == BleSession::Admit::Ignore) break;  // not the live connection
			slot_ = {f.conn, f.payload, f.len};
			if (a == BleSession::Admit::Busy) {
				reply(BleResult::Busy, ms);  // stale nonce or holding off; still answered once
				break;
			}
			pending_ = true;
			break;
		}
		default:
			break;  // PING, CHALLENGE, RESULT and HELLO-after-HELLO carry nothing for the door
	}
}

void Bridge::reply(BleResult code, uint32_t ms) {
	uint8_t msg[2];
	size_t n = bleEncodeResult(code, msg);
	send(Link::kResult, slot_.conn, msg, n);
	session_.answered(code == BleResult::Opened, ms);
	pending_ = false;
}

void Bridge::resetPulse(uint32_t ms) {
#ifdef BRIDGE_RESET
	if (resetting_) {
		if ((int32_t)(ms - resetReleaseMs_) >= 0) {
			pinMode(BRIDGE_RESET, INPUT);  // release: the pad floats high
			resetting_ = false;
		}
		return;
	}
	if (ms - lastHeardMs_ < kSilentBeforeResetMs || (lastResetMs_ && ms - lastResetMs_ < kResetGapMs)) return;
	LOG("Bridge silent for %lu s; resetting it.", (unsigned long)((ms - lastHeardMs_) / 1000));
	digitalWrite(BRIDGE_RESET, LOW);
	pinMode(BRIDGE_RESET, OUTPUT_OPEN_DRAIN);
	resetting_ = true;
	resetReleaseMs_ = ms + kResetLowMs;
	lastResetMs_ = ms;
	seen_ = online_ = false;
#else
	(void)ms;
#endif
}

void Bridge::service(uint32_t ms) {
	// While a request waits for loop() the decoder's buffer is in use; the UART buffer holds the rest.
	while (!pending_ && Serial2.available()) {
		if (decoder_.feed((uint8_t)Serial2.read())) handle(decoder_.frame(), ms);
	}
	if (ms - lastPingMs_ >= kPingEveryMs) {
		lastPingMs_ = ms;
		if (missed_ < 255) missed_++;
		if (missed_ > kMissedPongs && online_) {
			online_ = false;
			LOG("Bridge offline.");
		}
		send(Link::kPing, 0);
	}
	resetPulse(ms);
}

void Bridge::printStatus() const {
	if (!seen_) {
		LOG("bridge    : %s", online_ ? "online" : "no bridge heard");
		return;
	}
	LOG("bridge    : %s, firmware %s, link v%u%s, %lu BLE opens, %lu denied, %lu frames dropped", online_ ? "online" : "offline",
	    version_, linkVersion_, incompatible_ ? " (not supported)" : "", (unsigned long)opened_, (unsigned long)denied_,
	    (unsigned long)decoder_.dropped());
}
