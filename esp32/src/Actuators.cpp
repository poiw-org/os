#include "Actuators.h"
#include <Arduino.h>
#include "Pins.h"

namespace {
constexpr uint32_t kOpenMs = 1000;    // relay on time
constexpr uint32_t kBeepMs = 300;
constexpr uint32_t kSuccessMs = 1300;
constexpr uint32_t kDenyBeepsMs = 1300;
constexpr uint32_t kDenyLedMs = 3300;
// A relay must never stay energised because of a bug; this bounds any request.
constexpr uint32_t kMaxOpenMs = 5000;
}  // namespace

void Actuators::write(uint8_t pin, bool on) { digitalWrite(pin, on ? HIGH : LOW); }

void Actuators::begin() {
	// Outputs go low before anything else can run.
	for (uint8_t pin : {(uint8_t)DOOR_PIN, (uint8_t)SPEAKER, (uint8_t)IDLE_LED, (uint8_t)BUSY_LED,
	                    (uint8_t)ERROR_LED, (uint8_t)SUCCESS_LED}) {
		digitalWrite(pin, LOW);
		pinMode(pin, OUTPUT);
		digitalWrite(pin, LOW);
	}
}

void Actuators::grant() {
	denying_ = false;
	doorOn_ = true;
	doorUntil_ = millis() + (kOpenMs > kMaxOpenMs ? kMaxOpenMs : kOpenMs);
	denyStart_ = millis();  // the success timeline reuses this start time
}

void Actuators::deny() {
	if (doorOn_) return;  // never let a failed read interrupt a door that is opening
	denying_ = true;
	denyStart_ = millis();
}

void Actuators::reading(bool on) { reading_ = on; }
void Actuators::setOnline(bool online) { online_ = online; }

void Actuators::service() {
	uint32_t ms = millis();
	bool speaker = false, success = false, error = false;

	if (doorOn_) {
		if ((int32_t)(ms - doorUntil_) >= 0) doorOn_ = false;
		uint32_t t = ms - denyStart_;
		speaker = t < kBeepMs;
		success = t < kSuccessMs;
	}
	if (denying_) {
		uint32_t t = ms - denyStart_;
		if (t >= kDenyLedMs) denying_ = false;
		speaker = t < kDenyBeepsMs && (t % 500) < kBeepMs;
		error = t < kDenyLedMs;
	}

	write(DOOR_PIN, doorOn_);
	write(SPEAKER, speaker);
	write(SUCCESS_LED, success);
	write(ERROR_LED, error);
	write(BUSY_LED, reading_);
	write(IDLE_LED, online_ || (ms / 500) % 2);
}
