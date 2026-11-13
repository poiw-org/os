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
	// Master switch for the speaker. Its chords have knocked the PN532 off the I2C bus (wiring or
	// supply, cause not found), which is why the reader holds off during a denial while this is on.
	static constexpr bool kSound = true;
	void begin();
	void service();
	void grant();                // energise the relay and buzz for 5 s, the unlock phrase looping throughout
	void deny();                 // denial phrase and the error LED
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
	// Every chime's frequencies are scaled by this, in thousandths (1000 = as written), to suit the
	// speaker fitted. The caller persists it.
	void setPitch(uint16_t permille) { pitch_ = permille; toneHz_ = 0xFFFF; }
	uint16_t pitch() const { return pitch_; }
	// Plays one chime once, for trying them out. No relay or LEDs; any real event takes over.
	enum class Chime : uint8_t { Unlock, Deny, Alert, Write };
	void play(Chime c);
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
	uint16_t toneHz_ = 0, pitch_ = 1000;
	bool previewing_ = false;
	Chime preview_ = Chime::Unlock;
	uint32_t previewStart_ = 0;
	uint32_t doorUntil_ = 0, eventStart_ = 0, forceStart_ = 0;
};
