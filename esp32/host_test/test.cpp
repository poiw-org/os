// Host-side tests for the portable core. Build and run with `make test`.
#include <stdio.h>
#include <string.h>
#include "../src/Access.h"
#include "../src/BlacklistSet.h"
#include "../src/Crc.h"
#include "../src/Journal.h"
#include "../src/Jwt.h"
#include "../src/Ndef.h"
#include "../src/Pn532Frame.h"
#include "../src/Sha256.h"
#include "../src/SyncParser.h"
#include "../src/TimeUtil.h"

static int failures = 0, checks = 0;
#define CHECK(cond)                                                       \
	do {                                                                  \
		checks++;                                                         \
		if (!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
	} while (0)

static const char* SECRET = "test-secret-0123456789";
static const char* GOOD = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMTExMTExMS0yMjIyLTMzMzMtNDQ0NC01NTU1NTU1NTU1NTUiLCJqdGkiOiJhYWFhYWFhYS1iYmJiLWNjY2MtZGRkZC1lZWVlZWVlZWVlZWUiLCJhdWQiOiIwNDphMTpiMjpjMzpkNDplNTpmNiIsImlhdCI6MTcwMDAwMDAwMCwibmJmIjoxNzAwMDAwMTAwLCJleHAiOjE4MDAwMDAwMDB9.ZCcN-soRSAVt5aHA6L_ZcGh7UnjAbZbPvkSm1z1io4A";
static const char* WRONG_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMTExMTExMS0yMjIyLTMzMzMtNDQ0NC01NTU1NTU1NTU1NTUiLCJqdGkiOiJhYWFhYWFhYS1iYmJiLWNjY2MtZGRkZC1lZWVlZWVlZWVlZWUiLCJhdWQiOiIwNDphMTpiMjpjMzpkNDplNTpmNiIsImlhdCI6MTcwMDAwMDAwMCwibmJmIjoxNzAwMDAwMTAwLCJleHAiOjE4MDAwMDAwMDB9.lLxc2_l328ekowhAtSiB_7-vZqvL3HCrkw-RRFYu77s";
static const char* OLD_UID = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMTExMTExMS0yMjIyLTMzMzMtNDQ0NC01NTU1NTU1NTU1NTUiLCJqdGkiOiJhYWFhYWFhYS1iYmJiLWNjY2MtZGRkZC1lZWVlZWVlZWVlZWUiLCJhdWQiOiIweDA0IDB4QTEgMHhCMiAweEMzIiwiaWF0IjoxNzAwMDAwMDAwLCJuYmYiOjE3MDAwMDAxMDAsImV4cCI6MTgwMDAwMDAwMH0.hZ4ahZal44NEg3H-Bn9B-dikNXKTyy8pWzlIaCQsS74";
static const char* ALG_NONE = "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0.eyJzdWIiOiIxMTExMTExMS0yMjIyLTMzMzMtNDQ0NC01NTU1NTU1NTU1NTUiLCJqdGkiOiJhYWFhYWFhYS1iYmJiLWNjY2MtZGRkZC1lZWVlZWVlZWVlZWUiLCJhdWQiOiIwNDphMTpiMjpjMzpkNDplNTpmNiIsImlhdCI6MTcwMDAwMDAwMCwibmJmIjoxNzAwMDAwMTAwLCJleHAiOjE4MDAwMDAwMDB9.FU4OLZoWDXSTg4lFeDCwW1UFK3yPQIAXQks_ZT7cENM";

static void hex(const uint8_t* d, size_t n, char* out) {
	for (size_t i = 0; i < n; i++) snprintf(out + 2 * i, 3, "%02x", d[i]);
}

static void testHash() {
	uint8_t out[32];
	char h[65];
	Sha256 s;
	s.update((const uint8_t*)"abc", 3);
	s.final(out);
	hex(out, 32, h);
	CHECK(!strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));

	// RFC 4231 test cases 1 and 2.
	uint8_t key1[20];
	memset(key1, 0x0b, 20);
	hmacSha256(key1, 20, (const uint8_t*)"Hi There", 8, out);
	hex(out, 32, h);
	CHECK(!strcmp(h, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"));
	hmacSha256((const uint8_t*)"Jefe", 4, (const uint8_t*)"what do ya want for nothing?", 28, out);
	hex(out, 32, h);
	CHECK(!strcmp(h, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
	// Key longer than the block size is hashed first (RFC 4231 case 6).
	uint8_t key6[131];
	memset(key6, 0xaa, sizeof(key6));
	const char* m6 = "Test Using Larger Than Block-Size Key - Hash Key First";
	hmacSha256(key6, sizeof(key6), (const uint8_t*)m6, strlen(m6), out);
	hex(out, 32, h);
	CHECK(!strcmp(h, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"));
	// A message spanning several blocks.
	Sha256 big;
	for (int i = 0; i < 1000; i++) big.update((const uint8_t*)"a", 1);
	big.final(out);
	hex(out, 32, h);
	CHECK(!strcmp(h, "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3"));
}

static void testJwt() {
	KeyClaims k;
	size_t secretLen = strlen(SECRET);
	CHECK(jwtVerify(GOOD, strlen(GOOD), (const uint8_t*)SECRET, secretLen, k) == JwtResult::Ok);
	CHECK(!strcmp(k.jti, "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"));
	CHECK(!strcmp(k.sub, "11111111-2222-3333-4444-555555555555"));
	CHECK(k.audLen == 7 && k.aud[0] == 0x04 && k.aud[6] == 0xf6);
	CHECK(k.iat == 1700000000u && k.nbf == 1700000100u && k.exp == 1800000000u);

	CHECK(jwtVerify(WRONG_KEY, strlen(WRONG_KEY), (const uint8_t*)SECRET, secretLen, k) == JwtResult::BadSignature);
	CHECK(jwtVerify(ALG_NONE, strlen(ALG_NONE), (const uint8_t*)SECRET, secretLen, k) == JwtResult::BadClaims);  // right MAC, wrong alg
	CHECK(jwtVerify(OLD_UID, strlen(OLD_UID), (const uint8_t*)SECRET, secretLen, k) == JwtResult::Ok && k.audLen == 4 &&
	      k.aud[1] == 0xa1);

	// Tampering with one payload character must break the signature.
	char tampered[700];
	strcpy(tampered, GOOD);
	tampered[60] = tampered[60] == 'A' ? 'B' : 'A';
	CHECK(jwtVerify(tampered, strlen(tampered), (const uint8_t*)SECRET, secretLen, k) != JwtResult::Ok);
	CHECK(jwtVerify("", 0, (const uint8_t*)SECRET, secretLen, k) == JwtResult::Malformed);
	CHECK(jwtVerify("a.b", 3, (const uint8_t*)SECRET, secretLen, k) == JwtResult::Malformed);
	CHECK(jwtVerify("aaaaaaaaaaaaaaaaaaaaaaaa.bbbbbbbbbbbb", 37, (const uint8_t*)SECRET, secretLen, k) == JwtResult::Malformed);
	char huge[2000];
	memset(huge, 'a', sizeof(huge) - 1);
	huge[sizeof(huge) - 1] = 0;
	CHECK(jwtVerify(huge, strlen(huge), (const uint8_t*)SECRET, secretLen, k) == JwtResult::Malformed);

	uint8_t b[8];
	CHECK(base64UrlDecode("aGk", 3, b, sizeof(b)) == 2 && !memcmp(b, "hi", 2));
	CHECK(base64UrlDecode("a+b", 3, b, sizeof(b)) == -1);
}

static void testAccess() {
	KeyClaims k;
	memset(&k, 0, sizeof(k));
	k.audLen = 4;
	k.aud[0] = 1; k.aud[1] = 2; k.aud[2] = 3; k.aud[3] = 4;
	k.iat = 1000; k.nbf = 1000; k.exp = 2000;
	const uint8_t uid[4] = {1, 2, 3, 4};
	const uint8_t other[4] = {1, 2, 3, 5};
	const uint8_t shortUid[3] = {1, 2, 3};

	CHECK(decide(k, uid, 4, 1500, true, false) == Verdict::Allow);
	CHECK(decide(k, other, 4, 1500, true, false) == Verdict::SerialMismatch);
	CHECK(decide(k, shortUid, 3, 1500, true, false) == Verdict::SerialMismatch);
	CHECK(decide(k, uid, 4, 1500, true, true) == Verdict::Blacklisted);
	CHECK(decide(k, uid, 4, 2000, true, false) == Verdict::Expired);
	CHECK(decide(k, uid, 4, 1999, true, false) == Verdict::Allow);
	CHECK(decide(k, uid, 4, 900, true, false) == Verdict::NotYetActive);
	CHECK(decide(k, uid, 4, 971, true, false) == Verdict::Allow);  // within the leeway
	CHECK(decide(k, uid, 4, 969, true, false) == Verdict::NotYetActive);
	// A wrong tag wins over every other reason, a blacklist over expiry.
	CHECK(decide(k, other, 4, 5000, true, true) == Verdict::SerialMismatch);
	CHECK(decide(k, uid, 4, 5000, true, true) == Verdict::Blacklisted);
	// iat far in the future only counts when the clock is trusted.
	k.iat = 100000;
	k.nbf = 1000;
	CHECK(decide(k, uid, 4, 1500, true, false) == Verdict::IssuedInFuture);
	CHECK(decide(k, uid, 4, 1500, false, false) == Verdict::Allow);
	CHECK(reasonName(Verdict::Allow) == nullptr);
	CHECK(!strcmp(reasonName(Verdict::IssuedInFuture), "IssuingDateInTheFuture"));
}

static void testBlacklist() {
	BlacklistSet s;
	CHECK(!s.contains("a"));
	s.upsert("a", 100);
	s.upsert("b", 200);
	s.upsert("c", 300);
	CHECK(s.size() == 3 && s.contains("a") && s.contains("b") && s.contains("c") && !s.contains("d"));
	s.upsert("b", 250);
	CHECK(s.size() == 3);
	s.remove("b");
	s.remove("missing");
	CHECK(s.size() == 2 && !s.contains("b"));

	uint8_t buf[BlacklistSet::kMaxSerialized + 8];
	size_t n = s.serialize(buf, sizeof(buf));
	CHECK(n == 8 + 2 * 20 + 4);
	BlacklistSet t;
	CHECK(t.deserialize(buf, n) && t.size() == 2 && t.contains("a") && t.contains("c"));
	CHECK(s.serialize(buf, 10) == 0);

	// Any flipped bit, or a truncated file, is rejected.
	for (size_t i = 0; i < n; i++) {
		uint8_t copy[BlacklistSet::kMaxSerialized + 8];
		memcpy(copy, buf, n);
		copy[i] ^= 0x10;
		BlacklistSet u;
		CHECK(!u.deserialize(copy, n));
	}
	BlacklistSet v;
	CHECK(!v.deserialize(buf, n - 1) && !v.deserialize(buf, 0));

	CHECK(s.prune(150) == 1 && !s.contains("a") && s.contains("c"));

	// Capacity: the key that expires first is evicted.
	BlacklistSet big;
	char id[16];
	for (size_t i = 0; i < BlacklistSet::kCapacity; i++) {
		snprintf(id, sizeof(id), "k%zu", i);
		big.upsert(id, (uint32_t)(1000 + i));
	}
	CHECK(big.size() == BlacklistSet::kCapacity && !big.overflowed());
	big.upsert("late", 9999);
	CHECK(big.size() == BlacklistSet::kCapacity && big.overflowed() && big.contains("late") && !big.contains("k0"));
	big.upsert("early", 1);
	CHECK(!big.contains("early") && big.contains("k1"));
	uint8_t bigBuf[BlacklistSet::kMaxSerialized + 8];
	size_t bn = big.serialize(bigBuf, sizeof(bigBuf));
	BlacklistSet back;
	CHECK(back.deserialize(bigBuf, bn) && back.size() == BlacklistSet::kCapacity && back.contains("late"));
}

static void testSyncParser() {
	const char* body =
	    "{\"now\": 1767000000, \"total\": 3, \"blocked\": [[\"11111111-1111-1111-1111-111111111111\", 1800000000], "
	    "[\"legacy-firebase-id\", 1900000000],[\"33333333-3333-3333-3333-333333333333\",1850000000]]}";
	// Feed in awkward chunk sizes: tokens must survive being split anywhere.
	for (size_t chunk = 1; chunk <= 7; chunk++) {
		BlacklistSet s;
		SyncParser p(s);
		for (size_t i = 0; i < strlen(body); i += chunk) p.feed(body + i, i + chunk > strlen(body) ? strlen(body) - i : chunk);
		CHECK(p.complete() && p.now() == 1767000000u && !p.truncated());
		CHECK(s.size() == 3 && s.contains("legacy-firebase-id") && s.contains("33333333-3333-3333-3333-333333333333"));
	}
	BlacklistSet empty;
	SyncParser e(empty);
	const char* none = "{\"now\":5,\"total\":0,\"blocked\":[]}";
	e.feed(none, strlen(none));
	CHECK(e.complete() && e.now() == 5 && empty.size() == 0);

	BlacklistSet cut;
	SyncParser c(cut);
	c.feed("{\"now\": 5, \"tot", 15);
	CHECK(!c.complete());

	BlacklistSet more;
	SyncParser m(more);
	const char* trunc = "{\"now\":5,\"total\":900,\"blocked\":[[\"a\",10]]}";
	m.feed(trunc, strlen(trunc));
	CHECK(m.complete() && m.truncated());

	// A download that stops before the closing brace is not complete, wherever it stops.
	const char* whole = "{\"now\":5,\"total\":1,\"blocked\":[[\"a\",10]]}";
	for (size_t cutAt = 0; cutAt < strlen(whole); cutAt++) {
		BlacklistSet part;
		SyncParser q(part);
		q.feed(whole, cutAt);
		CHECK(!q.complete());
	}

	// Keys may arrive in any order.
	BlacklistSet reordered;
	SyncParser r(reordered);
	const char* alt = "{\"blocked\":[[\"x\",7]],\"now\":9,\"total\":1}";
	r.feed(alt, strlen(alt));
	CHECK(r.complete() && r.now() == 9 && reordered.contains("x"));
}

static void testTime() {
	CHECK(epochFromCivil(1970, 1, 1, 0, 0, 0) == 0);
	CHECK(epochFromCivil(2026, 11, 3, 15, 4, 5) == 1793718245u);
	char iso[21];
	formatIso(1793718245u, iso);
	CHECK(!strcmp(iso, "2026-11-03T15:04:05Z"));
	formatIso(951782400u, iso);  // leap day 2000
	CHECK(!strcmp(iso, "2000-02-29T00:00:00Z"));
	formatIso(4102444800u, iso);
	CHECK(!strcmp(iso, "2100-01-01T00:00:00Z"));
	CHECK(parseIso("2026-11-03T15:04:05Z") == 1793718245u);
	CHECK(parseIso("2026-11-03T15:04:05.123456+00:00") == 1793718245u);
	CHECK(parseIso("2026-11-03 17:04:05+02:00") == 1793718245u);
	CHECK(parseIso("2026-11-03T10:04:05-05") == 1793718245u);
	CHECK(parseIso("garbage") == 0 && parseIso("") == 0 && parseIso(nullptr) == 0);
	CHECK(parseIso("2026-13-03T15:04:05Z") == 0);
	CHECK(buildEpoch("Nov  3 2026", "15:04:05") == 1793718245u - 86400);
	CHECK(buildEpoch("junk", "15:04:05") == 0);

	uint8_t uid[10];
	CHECK(parseUid("04:a1:b2:c3", uid, sizeof(uid)) == 4 && uid[1] == 0xa1);
	CHECK(parseUid("04 A1 B2 C3", uid, sizeof(uid)) == 4);
	CHECK(parseUid("0x04 0xA1", uid, sizeof(uid)) == 2 && uid[0] == 4 && uid[1] == 0xa1);
	CHECK(parseUid("04:a", uid, sizeof(uid)) == -1);
	CHECK(parseUid("zz", uid, sizeof(uid)) == -1);
	CHECK(parseUid("", uid, sizeof(uid)) == -1);
	CHECK(parseUid("00:11:22:33:44:55:66:77:88:99:aa", uid, sizeof(uid)) == -1);
	char text[32];
	const uint8_t raw[3] = {0x04, 0xa1, 0x0f};
	CHECK(formatUid(raw, 3, text, sizeof(text)) == 8 && !strcmp(text, "04:a1:0f"));
	CHECK(formatUid(raw, 3, text, 5) == 0);

	char esc[64];
	CHECK(jsonEscape("p\"w\\d\n", esc, sizeof(esc)) == 9 && !strcmp(esc, "p\\\"w\\\\d\\n"));
	CHECK(jsonEscape("a\x01", esc, sizeof(esc)) == 7 && !strcmp(esc, "a\\u0001"));
	CHECK(jsonEscape("abcdef", esc, 4) == 0);
}

static void testJournal() {
	JournalRecord r;
	memset(&r, 0, sizeof(r));
	r.allowed = 1;
	r.ts = 1793718245u;
	r.uidLen = 4;
	r.uid[0] = 0x04; r.uid[1] = 0xa1; r.uid[2] = 0xb2; r.uid[3] = 0xc3;
	memcpy(r.key, "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", 36);
	r.keyLen = 36;
	r.flags = kOffline | kClockEstimated;
	CHECK(!journalValid(r));
	journalSeal(r);
	CHECK(journalValid(r));

	char json[400];
	size_t n = journalToJson(r, "main", json, sizeof(json));
	CHECK(n > 0);
	CHECK(strstr(json, "\"door_id\":\"main\"") && strstr(json, "\"type\":\"successful\"") &&
	      strstr(json, "\"reason\":null") && strstr(json, "\"key_id\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\"") &&
	      strstr(json, "\"serial_number\":\"04:a1:b2:c3\"") && strstr(json, "\"created_at\":\"2026-11-03T15:04:05Z\"") &&
	      strstr(json, "\"clock\":\"estimated\"") && strstr(json, "\"offline\":true"));
	CHECK(journalToJson(r, "main", json, 20) == 0);

	// A torn write fails the checksum.
	JournalRecord torn = r;
	torn.uid[0] ^= 1;
	CHECK(!journalValid(torn));

	JournalRecord bad;
	memset(&bad, 0, sizeof(bad));
	bad.verdict = (uint8_t)Verdict::Expired;
	bad.flags = kForced | kInvalidToken;
	journalSeal(bad);
	journalToJson(bad, "main", json, sizeof(json));
	CHECK(strstr(json, "\"type\":\"failed\"") && strstr(json, "\"reason\":\"KeyExpired\"") &&
	      strstr(json, "\"key_id\":null") && strstr(json, "\"serial_number\":null") && strstr(json, "\"forced\":true") &&
	      strstr(json, "invalid_token"));
	bad.verdict = 99;
	journalSeal(bad);
	journalToJson(bad, "main", json, sizeof(json));
	CHECK(strstr(json, "\"reason\":\"Unknown\""));
}

// Fake tag data area: `size` bytes are readable; `asked` records the most the walker requested.
struct FakeTag { size_t size; size_t asked; };
static bool fakeFill(void* ctx, size_t n) {
	FakeTag& t = *static_cast<FakeTag*>(ctx);
	if (n > t.asked) t.asked = n;
	return n <= t.size;
}

// A text record "en" + token, as phones write it, inside an NDEF TLV at `at`.
static size_t putTextTlv(uint8_t* a, size_t at, const char* token) {
	size_t tl = strlen(token);
	size_t payload = 3 + tl;
	size_t i = at;
	a[i++] = 0x03;
	if (payload + 4 < 0xFF) {  // short record: flags, type length, payload length, type
		a[i++] = (uint8_t)(payload + 4);
		a[i++] = 0xD1; a[i++] = 0x01; a[i++] = (uint8_t)payload; a[i++] = 'T';
	} else {                   // long record, 3-byte TLV length
		size_t msg = payload + 7;
		a[i++] = 0xFF; a[i++] = (uint8_t)(msg >> 8); a[i++] = (uint8_t)msg;
		a[i++] = 0xC1; a[i++] = 0x01;
		a[i++] = 0; a[i++] = 0; a[i++] = (uint8_t)(payload >> 8); a[i++] = (uint8_t)payload;
		a[i++] = 'T';
	}
	a[i++] = 0x02; a[i++] = 'e'; a[i++] = 'n';
	memcpy(a + i, token, tl);
	i += tl;
	a[i++] = 0xFE;
	return i;
}

static void testNdef() {
	uint8_t area[1024];
	const uint8_t* tok;
	size_t len;
	FakeTag tag;

	memset(area, 0, sizeof(area));
	size_t end = putTextTlv(area, 0, GOOD);
	tag = {end, 0};
	CHECK(ndefFindToken(area, fakeFill, &tag, tok, len) && len == strlen(GOOD) && !memcmp(tok, GOOD, len));
	CHECK(tag.asked <= end);  // never asks for bytes past the message

	// Lock control and memory control TLVs, then padding, before the message (as on NTAG tags).
	memset(area, 0, sizeof(area));
	const uint8_t pre[] = {0x01, 0x03, 0xA0, 0x10, 0x44, 0x02, 0x03, 0x00, 0x00, 0x00};
	memcpy(area, pre, sizeof(pre));
	end = putTextTlv(area, sizeof(pre), GOOD);
	tag = {end, 0};
	CHECK(ndefFindToken(area, fakeFill, &tag, tok, len) && len == strlen(GOOD));

	// A message over 254 bytes uses the long record and the 3-byte TLV length.
	char big[400];
	memset(big, 'A', sizeof(big) - 1);
	memcpy(big, "eyJ", 3);
	big[sizeof(big) - 1] = 0;
	memset(area, 0, sizeof(area));
	end = putTextTlv(area, 0, big);
	tag = {end, 0};
	CHECK(ndefFindToken(area, fakeFill, &tag, tok, len) && len == sizeof(big) - 1 && !memcmp(tok, big, len));

	// Not found: terminator first, empty tag, no JWT in the record, message cut off by the tag.
	memset(area, 0, sizeof(area));
	area[0] = 0xFE;
	tag = {sizeof(area), 0};
	CHECK(!ndefFindToken(area, fakeFill, &tag, tok, len));
	memset(area, 0, sizeof(area));
	tag = {16, 0};
	CHECK(!ndefFindToken(area, fakeFill, &tag, tok, len));
	end = putTextTlv(area, 0, "hello world");
	tag = {end, 0};
	CHECK(!ndefFindToken(area, fakeFill, &tag, tok, len));
	end = putTextTlv(area, 0, GOOD);
	tag = {end - 20, 0};
	CHECK(!ndefFindToken(area, fakeFill, &tag, tok, len));
	area[1] = 0xFF; area[2] = 0xFF; area[3] = 0xFF;  // a length that runs far past the tag
	tag = {sizeof(area), 0};
	CHECK(!ndefFindToken(area, fakeFill, &tag, tok, len));
}

static void testPn532Frame() {
	uint8_t f[16];
	const uint8_t firmware[] = {0x02};
	const uint8_t expectFirmware[] = {0x00, 0x00, 0xFF, 0x02, 0xFE, 0xD4, 0x02, 0x2A, 0x00};  // UM0701-02 example
	CHECK(pn532::buildFrame(f, firmware, 1) == 9 && !memcmp(f, expectFirmware, 9));
	const uint8_t list[] = {0x4A, 0x01, 0x00};
	size_t n = pn532::buildFrame(f, list, 3);
	CHECK(n == 11 && f[3] == 4 && (uint8_t)(f[3] + f[4]) == 0);
	uint8_t sum = 0;
	for (size_t i = 5; i < n - 1; i++) sum = (uint8_t)(sum + f[i]);  // TFI, payload and DCS add up to 0
	CHECK(sum == 0 && f[n - 1] == 0);
	CHECK(pn532::headerValid(f));
	uint8_t bad[5];
	memcpy(bad, f, 5);
	bad[4] ^= 1;
	CHECK(!pn532::headerValid(bad));
	CHECK(!pn532::headerValid(pn532::kAckFrame));  // LEN 00 / LCS FF marks an ACK, never a data frame
}

int main() {
	testNdef();
	testPn532Frame();
	testHash();
	testJwt();
	testAccess();
	testBlacklist();
	testSyncParser();
	testTime();
	testJournal();
	CHECK(crc32((const uint8_t*)"123456789", 9) == 0xCBF43926u);
	CHECK(crc16((const uint8_t*)"123456789", 9) == 0x29B1);
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
