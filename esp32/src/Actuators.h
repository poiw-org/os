// Door relay, speaker and LEDs. Everything is driven from service() with millis() deadlines, so
// opening the door never blocks the loop (the websocket and NFC keep running meanwhile).
#pragma once
#include <stdint.h>

class Actuators {
 public:
	void begin();
	void service();
	void grant();                // energise the relay and signal success
	void deny();                 // three beeps and the error LED
	void reading(bool on);       // busy LED while a tag is being read
	void setOnline(bool online); // idle LED: solid when connected, blinking when running offline
	bool busy() const { return doorOn_ || denying_; }

 private:
	void write(uint8_t pin, bool on);
	bool doorOn_ = false, denying_ = false, reading_ = false, online_ = false;
	uint32_t doorUntil_ = 0, denyStart_ = 0;
};
