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
// PWM for the LEDs. Channels 0 and 1 share the speaker's timer, whose frequency the tones retune.
constexpr uint8_t kLedChannel[3] = {2, 3, 4};  // red (ERROR), green (SUCCESS), blue (BUSY)
constexpr uint8_t kLedPin[3] = {ERROR_LED, SUCCESS_LED, BUSY_LED};
constexpr int32_t kDreamMax = 100;  // of 255, squared into duty: about 15% at the brightest
}  // namespace

void Actuators::write(uint8_t pin, bool on) { digitalWrite(pin, on ? HIGH : LOW); }

void Actuators::tone(uint16_t hz) {
	if (hz == toneHz_) return;
	toneHz_ = hz;
	ledcWriteTone(kToneChannel, hz);  // 0 stops the tone and leaves the pin low
}

void Actuators::begin() {
	// Outputs go low before anything else can run.
	for (uint8_t pin : {(uint8_t)DOOR_PIN, (uint8_t)SPEAKER, (uint8_t)BUSY_LED, (uint8_t)ERROR_LED,
	                    (uint8_t)SUCCESS_LED}) {
		digitalWrite(pin, LOW);
		pinMode(pin, OUTPUT);
		digitalWrite(pin, LOW);
	}
	for (uint8_t i = 0; i < 3; i++) {
		ledcSetup(kLedChannel[i], 5000, 8);
		ledcAttachPin(kLedPin[i], kLedChannel[i]);
		ledcWrite(kLedChannel[i], 0);
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

	if (kSound) tone(hz);
	write(DOOR_PIN, doorOn_);

	// Red, green and blue are the ERROR, SUCCESS and BUSY LEDs. Events drive them at full duty; with
	// nothing going on they daydream.
	uint8_t duty[3] = {0, 0, 0};
	if (error || success || reading_ || warning) {
		duty[0] = error ? 255 : 0;
		duty[1] = success ? 255 : 0;
		duty[2] = reading_ || warning ? 255 : 0;
		dreamDur_ = 0;  // the next quiet moment starts a fresh drift
	} else if (writer_ == Writer::Off) {
		daydream(ms, duty);
	}
	for (uint8_t i = 0; i < 3; i++) ledcWrite(kLedChannel[i], duty[i]);
}

// A slow drift between random dim colours. Levels are 0..kDreamMax and squared into PWM duty, so low
// levels are nearly dark; one target in four is black, held for longer, so the lamp sometimes goes out.
void Actuators::daydream(uint32_t ms, uint8_t duty[3]) {
	if (ms - dreamStart_ >= dreamDur_) {
		memcpy(dreamFrom_, dreamTo_, sizeof dreamFrom_);
		dreamStart_ = ms;
		if (random(4) == 0) {
			memset(dreamTo_, 0, sizeof dreamTo_);
			dreamDur_ = 3000 + random(4000);
		} else {
			for (uint8_t& v : dreamTo_) v = random(kDreamMax + 1);
			dreamDur_ = 2000 + random(3000);
		}
	}
	int32_t t = (int32_t)((ms - dreamStart_) * 256 / dreamDur_);
	for (uint8_t i = 0; i < 3; i++) {
		int32_t v = dreamFrom_[i] + ((int32_t)dreamTo_[i] - dreamFrom_[i]) * t / 256;
		duty[i] = (uint8_t)(v * v / 255);
	}
}
