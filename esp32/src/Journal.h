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
	kOffline = 8          // the door had no cloud connection at the time
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
	uint8_t reserved[6];
	uint16_t crc;
};
static_assert(sizeof(JournalRecord) == 64, "journal records are 64 bytes");

constexpr uint8_t kJournalMagic = 0xD0;

void journalSeal(JournalRecord& r);
bool journalValid(const JournalRecord& r);

// One element of the JSON array posted to /rest/v1/entries. Returns the length or 0 if `cap` is
// too small.
size_t journalToJson(const JournalRecord& r, const char* doorId, char* out, size_t cap);
