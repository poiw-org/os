// PN532 NFC reader. Hands out each presented tag once, with its UID and the key token stored in
// its first NDEF record. NTAG/Ultralight tags are read directly, 16 bytes per exchange.
#pragma once
#include <PN532.h>
#include <PN532_I2C.h>
#include <NfcAdapter.h>
#include <Wire.h>
#include "Jwt.h"

struct TagRead {
	uint8_t uid[10];
	uint8_t uidLen;
	bool hasToken;
	char token[kMaxTokenLen + 1];
	size_t tokenLen;
};

class Reader {
 public:
	Reader() : i2c_(Wire), pn532_(i2c_), nfc_(i2c_) {}
	void begin();
	// True when a tag that was not on the reader before has been read into `out`.
	bool poll(TagRead& out);
	// False while the PN532 is missing or unresponsive. The door keeps running without it; the reader
	// re-probes on its own, so other unlock methods are never blocked by the NFC hardware.
	bool available() const { return ready_; }

 private:
	PN532_I2C i2c_;
	PN532 pn532_;
	NfcAdapter nfc_;
	bool present_ = false;
	uint32_t lastSeenMs_ = 0;
	uint32_t lastCheckMs_ = 0;
	bool ready_ = false;
	bool probe();
	bool readPages(uint8_t page, uint8_t* out);
	void readType2(TagRead& out);
};
