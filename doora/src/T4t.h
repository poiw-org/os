// The door as an NFC Forum Type 4 tag, so a phone that taps it reads a link (NFC Forum T4T 2.0).
// Pure: it answers one command APDU at a time, so it is unit-tested on the workstation.
#pragma once
#include <stddef.h>
#include <stdint.h>

struct T4tTag {
	// The NDEF file: 2-byte length, then the message (see ndefEncodeUri()).
	uint8_t ndef[64];
	size_t ndefLen = 0;
	// Session state, cleared by begin() whenever a phone activates the tag.
	enum class File : uint8_t { None, Cc, Ndef } file = File::None;
	bool appSelected = false;
	bool served = false;  // the phone has read the whole NDEF file

	void begin() {
		file = File::None;
		appSelected = false;
		served = false;
	}
};

// Largest data field of one READ BINARY answer (the CC's MLe). Phones read the file in chunks of
// at most this.
constexpr size_t kT4tMaxRead = 52;

// Writes the response APDU (data and status word) to `out`; returns its length. `cap` must be at
// least kT4tMaxRead + 2.
size_t t4tRespond(T4tTag& tag, const uint8_t* apdu, size_t n, uint8_t* out, size_t cap);
