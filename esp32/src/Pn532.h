// Minimal PN532 driver over I2C: find an ISO14443A tag and exchange frames with it. Written for
// this door instead of using the elechouse library, whose defaults ask the chip to look for a tag
// forever and whose timeouts then abandon that command mid-flight, leaving the chip busy for the
// next one. Here every command is bounded, finishes on its own, and a timed-out one is aborted.
// No heap, no String; the only buffers are small ones on the caller's stack.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "SoftI2c.h"

class Pn532 {
 public:
	struct Target {
		uint8_t uid[10];
		uint8_t uidLen;
		uint8_t sak;  // SEL_RES: 0x00 NFC Forum Type 2 (NTAG, Ultralight), bit 3 MIFARE Classic
	};
	enum class Detect : uint8_t { None, Found, Error };

	// Negative results of exchange().
	static constexpr int kErrBus = -1;      // no answer on the bus
	static constexpr int kErrTimeout = -2;  // the chip did not finish in time (the command was aborted)
	static constexpr int kErrFrame = -3;    // malformed or oversized response
	static constexpr int kErrTag = -4;      // the tag did not answer, or refused (e.g. wrong key)

	// Resets the bus, finds the chip and configures it. False when it is missing.
	bool begin(uint8_t sda, uint8_t scl);
	uint32_t version() const { return version_; }  // IC << 24 | firmware major << 16 | minor << 8
	bool ping();

	// Looks for one tag, for at most a few milliseconds.
	Detect detect(Target& out);
	// Sends `data` to the tag found by detect(). On success returns the number of bytes written to
	// `out`; `cap` must also have room for one status byte (data length + 1).
	int exchange(const uint8_t* data, size_t n, uint8_t* out, size_t cap);

 private:
	uint32_t firmware();
	int command(const uint8_t* payload, size_t n, uint8_t* resp, size_t cap, uint32_t timeoutMs);
	bool writeFrame(const uint8_t* payload, size_t n);
	bool waitReady(uint32_t timeoutMs);
	bool readAck();
	int readResponse(uint8_t cmd, uint8_t* resp, size_t cap);
	int failure() const { return i2c_.stuck() ? kErrBus : kErrTimeout; }
	void abort();

	SoftI2c i2c_;
	uint32_t version_ = 0;
};
