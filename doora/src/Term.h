// The console's one stream: output goes to USB and to an approved Bluetooth link, input comes from either.
#pragma once
#include <Arduino.h>
#include "Bt.h"
#include "RemoteConsole.h"

class TermStream : public Stream {
 public:
	using Print::write;
	int available() override { return Serial.available() + Bt::available(); }
	int read() override { return Serial.available() ? Serial.read() : Bt::read(); }
	int peek() override { return -1; }  // nothing in the console peeks
	void flush() override { Serial.flush(); }
	size_t write(uint8_t c) override { return write(&c, 1); }
	size_t write(const uint8_t* data, size_t len) override {
		Serial.write(data, len);
		Bt::write(data, len);
		RemoteConsole::write(data, len);
		return len;
	}
};

inline TermStream Term;
