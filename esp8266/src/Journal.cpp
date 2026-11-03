#include "Journal.h"
#include <stdio.h>
#include <string.h>
#include "Crc.h"
#include "TimeUtil.h"

void journalSeal(JournalRecord& r) {
	r.magic = kJournalMagic;
	r.crc = crc16((const uint8_t*)&r, sizeof(r) - 2);
}

bool journalValid(const JournalRecord& r) {
	return r.magic == kJournalMagic && r.uidLen <= sizeof(r.uid) && r.keyLen <= sizeof(r.key) &&
	       r.crc == crc16((const uint8_t*)&r, sizeof(r) - 2);
}

size_t journalToJson(const JournalRecord& r, const char* doorId, char* out, size_t cap) {
	char iso[21], uid[32], key[40];
	formatIso(r.ts, iso);
	formatUid(r.uid, r.uidLen, uid, sizeof(uid));
	memcpy(key, r.key, r.keyLen);
	key[r.keyLen] = 0;
	// Tokens are signature-checked before they are journaled, but never trust text in JSON.
	char keyEsc[80], doorEsc[80];
	if (!jsonEscape(key, keyEsc, sizeof(keyEsc)) && r.keyLen) return 0;
	if (!jsonEscape(doorId, doorEsc, sizeof(doorEsc))) return 0;

	const char* reason = r.allowed ? nullptr : reasonName((Verdict)r.verdict);
	if (!r.allowed && !reason) reason = "Unknown";
	char reasonJson[40];
	if (reason) snprintf(reasonJson, sizeof(reasonJson), "\"%s\"", reason);
	else snprintf(reasonJson, sizeof(reasonJson), "null");

	char keyJson[96], uidJson[48];
	if (r.keyLen) snprintf(keyJson, sizeof(keyJson), "\"%s\"", keyEsc); else snprintf(keyJson, sizeof(keyJson), "null");
	if (r.uidLen) snprintf(uidJson, sizeof(uidJson), "\"%s\"", uid); else snprintf(uidJson, sizeof(uidJson), "null");

	int n = snprintf(out, cap,
	                 "{\"door_id\":\"%s\",\"type\":\"%s\",\"reason\":%s,\"key_id\":%s,\"serial_number\":%s,"
	                 "\"created_at\":\"%s\",\"metadata\":{\"clock\":\"%s\",\"offline\":%s%s%s}}",
	                 doorEsc, r.allowed ? "successful" : "failed", reasonJson, keyJson, uidJson, iso,
	                 (r.flags & kClockEstimated) ? "estimated" : "synced", (r.flags & kOffline) ? "true" : "false",
	                 (r.flags & kForced) ? ",\"forced\":true" : "",
	                 (r.flags & kInvalidToken) ? ",\"cause\":\"invalid_token\"" : "");
	return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
}
