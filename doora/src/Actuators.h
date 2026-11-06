// Door relay, speaker and LEDs. Everything is driven from service() with millis() deadlines, so
// opening the door never blocks the loop (the websocket and NFC keep running meanwhile).
//
// The speaker must be a passive buzzer (or piezo): chimes are tones from the ESP32's LEDC timer.
// The ERROR, SUCCESS and BUSY LEDs are the red, green and blue of one RGB LED, driven by PWM.
#pragma once
#include <stdint.h>
#include "WriteState.h"

class Actuators {
 public:
	// The speaker is silent: its chords knocked the PN532 off the I2C bus (wiring or supply, not found
	// yet). Every sound is still computed from the same timeline, so turning this on restores them all
	// (and then the reader's hold-off during a denial matters again).
	static constexpr bool kSound = false;
	void begin();
	void service();
	void grant();                // energise the relay and buzz for 3 s
	void deny();                 // sad chord and the error LED
	void forceOpen();            // attention tone for 3 s, then grant(); ignored while one is pending
	// True once per force open whose countdown ended and whose relay was just energised.
	bool takeForcedOpen() {
		bool v = forcedOpened_;
		forcedOpened_ = false;
		return v;
	}
	// Sounds for writer mode, from the writer session's state: the attention tone while waiting for
	// the tag, a faster one while writing, the happy chord when done, the sad one when it failed.
	void writer(WriteState s);
	void reading(bool on);       // busy LED while a tag is being read
	bool busy() const { return doorOn_ || denying_ || forcing_; }

 private:
	void write(uint8_t pin, bool on);
	void tone(uint16_t hz);
	void daydream(uint32_t ms, uint8_t duty[3]);
	bool doorOn_ = false, denying_ = false, forcing_ = false, forcedOpened_ = false;
	bool reading_ = false;
	uint8_t dreamFrom_[3] = {0, 0, 0}, dreamTo_[3] = {0, 0, 0};
	uint32_t dreamStart_ = 0, dreamDur_ = 0;
	enum class Writer : uint8_t { Off, Waiting, Writing, Finished } writer_ = Writer::Off;
	WriteState writerSeen_ = WriteState::Idle;
	uint32_t writerStart_ = 0;
	uint16_t toneHz_ = 0;
	uint32_t doorUntil_ = 0, eventStart_ = 0, forceStart_ = 0;
};
