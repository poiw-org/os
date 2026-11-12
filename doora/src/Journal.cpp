#include "Journal.h"
#include <stdio.h>
#include <string.h>
#include "Crc.h"
#include "Jwt.h"
#include "TimeUtil.h"

void journalSetClaims(JournalRecord& r, const KeyClaims& key, bool verified) {
	r.keyLen = (uint8_t)strnlen(key.jti, sizeof(r.key));
	memcpy(r.key, key.jti, r.keyLen);
	r.subLen = (uint8_t)strnlen(key.sub, sizeof(r.sub));
	memcpy(r.sub, key.sub, r.subLen);
	r.audLen = key.audLen <= sizeof(r.aud) ? key.audLen : sizeof(r.aud);
	memcpy(r.aud, key.aud, r.audLen);
	r.iat = key.iat;
	r.nbf = key.nbf;
	r.exp = key.exp;
	r.flags |= kClaims;
	if (!verified) r.flags |= kUnverified;
}

void journalSeal(JournalRecord& r) {
	r.magic = kJournalMagic;
	r.crc = crc16((const uint8_t*)&r, sizeof(r) - 2);
}

bool journalValid(const JournalRecord& r) {
	return r.magic == kJournalMagic && r.uidLen <= sizeof(r.uid) && r.keyLen <= sizeof(r.key) &&
	       r.subLen <= sizeof(r.sub) && r.audLen <= sizeof(r.aud) &&
	       r.crc == crc16((const uint8_t*)&r, sizeof(r) - 2);
}

size_t journalToJson(const JournalRecord& r, const char* doorId, char* out, size_t cap) {
	char iso[21], uid[32], key[40];
	formatIso(r.ts, iso);
	formatUid(r.uid, r.uidLen, uid, sizeof(uid));
	memcpy(key, r.key, r.keyLen);
	key[r.keyLen] = 0;
	// The jti may come from a token that failed its signature check: never trust text in JSON.
	char keyEsc[80], doorEsc[80];
	if (!jsonEscape(key, keyEsc, sizeof(keyEsc)) && r.keyLen) return 0;
	if (!jsonEscape(doorId, doorEsc, sizeof(doorEsc))) return 0;

	const char* reason = r.allowed ? nullptr : reasonName((Verdict)r.verdict);
	if (!r.allowed && !reason) reason = "Unknown";
	char reasonJson[40];
	if (reason) snprintf(reasonJson, sizeof(reasonJson), "\"%s\"", reason);
	else snprintf(reasonJson, sizeof(reasonJson), "null");

	// An unverified token's jti is not trusted as a key id; it only appears among the claims.
	bool trusted = !(r.flags & kUnverified);
	char keyJson[96], uidJson[48];
	if (r.keyLen && trusted) snprintf(keyJson, sizeof(keyJson), "\"%s\"", keyEsc); else snprintf(keyJson, sizeof(keyJson), "null");
	if (r.uidLen) snprintf(uidJson, sizeof(uidJson), "\"%s\"", uid); else snprintf(uidJson, sizeof(uidJson), "null");

	// What the token says about itself, so a tag whose key is gone can still be identified.
	char claims[280] = "";
	if (r.flags & kClaims) {
		char sub[40], subEsc[80], aud[32];
		memcpy(sub, r.sub, r.subLen);
		sub[r.subLen] = 0;
		if (!jsonEscape(sub, subEsc, sizeof(subEsc))) subEsc[0] = 0;
		formatUid(r.aud, r.audLen, aud, sizeof(aud));
		int c = snprintf(claims, sizeof(claims),
		                 ",\"claims\":{\"jti\":\"%s\",\"sub\":\"%s\",\"aud\":\"%s\",\"iat\":%lu,\"nbf\":%lu,\"exp\":%lu,"
		                 "\"verified\":%s}",
		                 r.keyLen ? keyEsc : "", subEsc, r.audLen ? aud : "", (unsigned long)r.iat, (unsigned long)r.nbf,
		                 (unsigned long)r.exp, trusted ? "true" : "false");
		if (c <= 0 || (size_t)c >= sizeof(claims)) claims[0] = 0;
	}

	int n = snprintf(out, cap,
	                 "{\"door_id\":\"%s\",\"type\":\"%s\",\"reason\":%s,\"key_id\":%s,\"serial_number\":%s,"
	                 "\"created_at\":\"%s\",\"metadata\":{\"clock\":\"%s\",\"offline\":%s%s%s%s}}",
	                 doorEsc, r.allowed ? "successful" : "failed", reasonJson, keyJson, uidJson, iso,
	                 (r.flags & kClockEstimated) ? "estimated" : "synced", (r.flags & kOffline) ? "true" : "false",
	                 (r.flags & kForced) ? ",\"forced\":true" : "",
	                 (r.flags & kInvalidToken) ? ",\"cause\":\"invalid_token\"" : "", claims);
	return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
}
