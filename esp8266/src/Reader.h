// PN532 NFC reader. Hands out each presented tag once, with its UID and the key token stored in
// its first NDEF record.
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
	// Checks the PN532 still answers and re-initialises it if not. False if it keeps failing, which
	// the caller should treat as "restart".
	bool healthy();

 private:
	PN532_I2C i2c_;
	PN532 pn532_;
	NfcAdapter nfc_;
	bool present_ = false;
	uint32_t lastSeenMs_ = 0;
	uint32_t lastCheckMs_ = 0;
	uint8_t failures_ = 0;
};
