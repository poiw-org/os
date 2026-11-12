// A door event waiting to be uploaded. Fixed-size records make the on-flash queue trivial to
// append to and to repair after a power cut: a torn record fails its checksum and is skipped.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Access.h"

enum JournalFlags : uint8_t {
	kClockEstimated = 1,  // the timestamp is a lower bound, not a synced time
	kForced = 2,          // opened by a force-open command, not a key
	kInvalidToken = 4,    // the tag did not carry a valid key token
	kOffline = 8,         // the door had no cloud connection at the time
	kClaims = 16,         // sub/iat/nbf/exp/aud below were read from the tag's token
	kUnverified = 32,     // ...but its signature did not verify, so they only help identify the tag
	kBle = 64             // opened (or refused) over Bluetooth by a phone, not a tag; uid is empty
};

struct __attribute__((packed)) JournalRecord {
	uint8_t magic;
	uint8_t allowed;  // 1 = door opened
	uint8_t verdict;  // Verdict value for failures
	uint8_t flags;
	uint32_t ts;
	uint8_t uidLen;
	uint8_t uid[10];
	uint8_t keyLen;
	char key[36];     // the token's jti, not NUL terminated
	uint8_t subLen;
	char sub[36];     // the token's sub (user id), not NUL terminated
	uint8_t audLen;
	uint8_t aud[10];  // the serial number the token was issued for
	uint32_t iat, nbf, exp;
	uint8_t reserved[10];
	uint16_t crc;
};
static_assert(sizeof(JournalRecord) == 128, "journal records are 128 bytes");

constexpr uint8_t kJournalMagic = 0xD0;

struct KeyClaims;
// Copies what the tag's token says about itself into the record, so a tag whose key is deleted or
// unknown can still be identified. `verified` is false for a token that failed its signature check.
void journalSetClaims(JournalRecord& r, const KeyClaims& key, bool verified);

void journalSeal(JournalRecord& r);
bool journalValid(const JournalRecord& r);

// One element of the JSON array posted to /rest/v1/entries. Returns the length or 0 if `cap` is
// too small.
size_t journalToJson(const JournalRecord& r, const char* doorId, char* out, size_t cap);
