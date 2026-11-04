#include "Actuators.h"
#include <Arduino.h>
#include "Chimes.h"
#include "Pins.h"

namespace {
// Relay on time (the strike buzzes and the speaker plays the unlock tone meanwhile), the same for a
// tap and a force open. Fixed, so a bug can never keep the relay energised.
constexpr uint32_t kOpenMs = 3000;
constexpr uint32_t kSuccessMs = 1300;
constexpr uint32_t kDenyLedMs = 3300;
constexpr uint8_t kToneChannel = 0;
}  // namespace

void Actuators::write(uint8_t pin, bool on) { digitalWrite(pin, on ? HIGH : LOW); }

void Actuators::tone(uint16_t hz) {
	if (hz == toneHz_) return;
	toneHz_ = hz;
	ledcWriteTone(kToneChannel, hz);  // 0 stops the tone and leaves the pin low
}

void Actuators::begin() {
	// Outputs go low before anything else can run.
	for (uint8_t pin : {(uint8_t)DOOR_PIN, (uint8_t)SPEAKER, (uint8_t)IDLE_LED, (uint8_t)BUSY_LED,
	                    (uint8_t)ERROR_LED, (uint8_t)SUCCESS_LED}) {
		digitalWrite(pin, LOW);
		pinMode(pin, OUTPUT);
		digitalWrite(pin, LOW);
	}
	ledcSetup(kToneChannel, 2000, 8);
	ledcAttachPin(SPEAKER, kToneChannel);
	ledcWriteTone(kToneChannel, 0);
}

void Actuators::grant() {
	denying_ = false;
	doorOn_ = true;
	doorUntil_ = millis() + kOpenMs;
	eventStart_ = millis();
}

void Actuators::deny() {
	if (doorOn_ || forcing_) return;  // never let a failed read interrupt a door that is opening
	denying_ = true;
	eventStart_ = millis();
}

void Actuators::forceOpen() {
	if (forcing_) return;
	forcing_ = true;
	forceStart_ = millis();
}

void Actuators::writer(WriteState s) {
	if (s == writerSeen_) return;
	writerSeen_ = s;
	writerStart_ = millis();
	switch (s) {
		case WriteState::Armed:
		case WriteState::WrongTag: writer_ = Writer::Waiting; break;
		case WriteState::Writing: writer_ = Writer::Writing; break;
		case WriteState::Done: writer_ = Writer::Finished; break;
		case WriteState::TooSmall:
		case WriteState::Unsupported:
		case WriteState::Failed:
		case WriteState::Rejected:
			writer_ = Writer::Off;
			deny();
			break;
		default: writer_ = Writer::Off; break;
	}
}

void Actuators::reading(bool on) { reading_ = on; }
void Actuators::setOnline(bool online) { online_ = online; }

void Actuators::service() {
	uint32_t ms = millis();
	uint16_t hz = 0;
	bool success = false, error = false, warning = false;

	if (writer_ != Writer::Off) {
		uint32_t t = ms - writerStart_;
		if (writer_ == Writer::Waiting) hz = Chimes::attention(t);
		else if (writer_ == Writer::Writing) hz = Chimes::writing(t);
		else if (t < kSuccessMs) {
			hz = Chimes::unlocked(t);
			success = true;
		} else {
			writer_ = Writer::Off;
		}
	}
	if (forcing_) {
		uint32_t t = ms - forceStart_;
		if (t >= Chimes::kForceOpenDelayMs) {
			forcing_ = false;
			forcedOpened_ = true;
			grant();
		} else {
			hz = Chimes::attention(t);
			warning = hz != 0;
		}
	}
	if (doorOn_) {
		if ((int32_t)(ms - doorUntil_) >= 0) doorOn_ = false;
		uint32_t t = ms - eventStart_;
		hz = Chimes::unlocked(t);
		success = t < kSuccessMs;
	}
	if (denying_) {
		uint32_t t = ms - eventStart_;
		if (t >= kDenyLedMs) denying_ = false;
		hz = t < Chimes::kDeniedMs ? Chimes::denied(t) : 0;
		error = t < kDenyLedMs;
	}

	tone(hz);
	write(DOOR_PIN, doorOn_);
	write(SUCCESS_LED, success);
	write(ERROR_LED, error);
	write(BUSY_LED, reading_ || warning);
	write(IDLE_LED, online_ || (ms / 500) % 2);
}
