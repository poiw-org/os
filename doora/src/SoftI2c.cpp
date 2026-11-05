#include "SoftI2c.h"
#include <Arduino.h>
#include <driver/gpio.h>

namespace {
// Bus timing, kept inside the PN532's 400 kHz (fast mode) limits. START, STOP and the low half of
// each clock take kHalfUs (fast mode needs 1.3 us low); the high half of a data clock only needs
// 0.6 us, so it takes kHighUs. The master waits for SCL to really rise before timing the high half,
// so slow pull-ups stretch the clock instead of shortening it.
constexpr uint32_t kHalfUs = 2;
constexpr uint32_t kHighUs = 1;
// Longest a slave may hold SCL low before the bus counts as stuck.
constexpr uint32_t kStretchUs = 20000;

inline void halfBit() { delayMicroseconds(kHalfUs); }
inline void highHalf() { delayMicroseconds(kHighUs); }
inline void level(int pin, int v) { gpio_set_level((gpio_num_t)pin, v); }
inline int sample(int pin) { return gpio_get_level((gpio_num_t)pin); }
}  // namespace

void SoftI2c::begin(uint8_t sda, uint8_t scl) {
	sda_ = sda;
	scl_ = scl;
	gpio_config_t cfg = {};
	cfg.pin_bit_mask = (1ULL << sda) | (1ULL << scl);
	cfg.mode = GPIO_MODE_INPUT_OUTPUT_OD;
	cfg.pull_up_en = GPIO_PULLUP_ENABLE;  // on top of the module's own pull-ups
	gpio_config(&cfg);
	level(sda_, 1);
	level(scl_, 1);
	recover();
}

// A reset in the middle of a transfer can leave a slave driving SDA low, waiting for clocks.
// Nine pulses finish whatever byte it was sending, then a STOP resets the bus.
void SoftI2c::recover() {
	stuck_ = false;
	level(sda_, 1);
	for (int i = 0; i < 9 && !sample(sda_); i++) {
		level(scl_, 0);
		halfBit();
		halfBit();
		level(scl_, 1);
		halfBit();
		halfBit();
	}
	level(sda_, 0);
	halfBit();
	level(scl_, 1);
	halfBit();
	level(sda_, 1);
	halfBit();
}

// Lets SCL go high and waits while the slave stretches it. Once the bus is stuck it fails at once,
// so the rest of a transfer (read, write, stop) costs nothing instead of one timeout per call;
// recover() clears it.
bool SoftI2c::sclRelease() {
	if (stuck_) return false;
	level(scl_, 1);
	uint32_t t0 = micros();
	while (!sample(scl_)) {
		// Look at the line once more after the deadline: a flash write on the other core can freeze
		// this one between the sample and the clock check, and a stretch that ended meanwhile is not
		// a stuck bus. Giving up there abandoned the PN532 half way through a byte, which can leave it
		// holding SCL low until it loses power.
		if (micros() - t0 > kStretchUs && !sample(scl_)) {
			stuck_ = true;
			return false;
		}
	}
	return true;
}

bool SoftI2c::start(uint8_t addr7, bool read) {
	if (stuck_) return false;
	level(sda_, 1);
	if (!sclRelease()) return false;
	halfBit();
	level(sda_, 0);  // START: SDA falls while SCL is high
	halfBit();
	level(scl_, 0);
	if (write((uint8_t)(addr7 << 1 | (read ? 1 : 0)))) return true;
	stop();
	return false;
}

bool SoftI2c::write(uint8_t b) {
	for (int i = 7; i >= 0; i--) {
		level(sda_, (b >> i) & 1);
		halfBit();
		if (!sclRelease()) return false;
		highHalf();
		level(scl_, 0);
	}
	level(sda_, 1);  // hand SDA to the slave for its ACK
	halfBit();
	if (!sclRelease()) return false;
	highHalf();
	bool acked = !sample(sda_);
	level(scl_, 0);
	return acked;
}

uint8_t SoftI2c::read(bool ack) {
	uint8_t b = 0;
	level(sda_, 1);
	for (int i = 0; i < 8; i++) {
		halfBit();
		if (!sclRelease()) return 0xFF;
		highHalf();
		b = (uint8_t)(b << 1 | sample(sda_));
		level(scl_, 0);
	}
	level(sda_, ack ? 0 : 1);
	halfBit();
	if (!sclRelease()) return 0xFF;
	highHalf();
	level(scl_, 0);
	level(sda_, 1);
	return b;
}

void SoftI2c::stop() {
	level(sda_, 0);
	halfBit();
	if (!sclRelease()) return;
	halfBit();
	level(sda_, 1);  // STOP: SDA rises while SCL is high
	halfBit();
}

int SoftI2c::lines() {
	level(sda_, 1);
	level(scl_, 1);
	delayMicroseconds(10);
	return sample(sda_) | sample(scl_) << 1;
}
