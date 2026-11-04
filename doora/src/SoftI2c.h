// Bit-banged I2C master on two open-drain GPIOs. The PN532 stretches SCL while it works; a
// software master simply waits for the line, which is how the ESP8266 core drove the same chip.
// Single-threaded: only the reader task touches the bus.
#pragma once
#include <stdint.h>

class SoftI2c {
 public:
	// Claims the pins and frees a bus that a reset left half-way through a byte.
	void begin(uint8_t sda, uint8_t scl);
	// START and address byte. False, with the bus released, when nobody acknowledges.
	bool start(uint8_t addr7, bool read);
	bool write(uint8_t b);  // true when the byte was acknowledged
	uint8_t read(bool ack);
	void stop();
	// SCL was held low for longer than any slave should: every call fails until begin().
	bool stuck() const { return stuck_; }

 private:
	bool sclRelease();
	void recover();
	int sda_ = -1;
	int scl_ = -1;
	bool stuck_ = false;
};
