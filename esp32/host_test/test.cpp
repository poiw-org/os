// Host-side tests for the portable core. Build and run with `make test`.
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "../src/AccessToken.h"
#include "../src/Access.h"
#include "../src/Backoff.h"
#include "../src/BlacklistSet.h"
#include "../src/Chimes.h"
#include "../src/Crc.h"
#include "../src/Journal.h"
#include "../src/Jwt.h"
#include "../src/Ndef.h"
#include "../src/Pn532Frame.h"
#include "../src/Sha256.h"
#include "../src/SyncParser.h"
#include "../src/TimeUtil.h"
#include "../src/WsFrame.h"

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

static VerifyKeys hsKeys() { return VerifyKeys{(const uint8_t*)SECRET, strlen(SECRET), nullptr}; }

// v2 tokens: RS256 signed with a throwaway 2048-bit key (generated with node's crypto module).
static const char* RSA_N = "lN-_bUdgw9pu-N_S_sieK3dnECFbyJDfxMERNA3HU26oevw5Hs26vd3dCXCwQEv3B8TzNKIQd011jxiLG0Ey32w3aYQlchX3zsNo8rY0AuB8yq_GmIdDbGEwVvjBwthcCXimvKjxxFt_4xrz3ayWj_x9jiSyDDYgNTwPJPa3EuM-AsJ73b-2y0SrMfwx8U3GYi5i1u5duKmzGqHc5FyoBFFO3dPYHmyucVV2-XgBvC3-YRFUTpqSRUSPSKfqwJt-Pr-2yhSzOwIJqQxZJk5geaLw2F-2f8sxdOBVC4F0sP3_FcWHzhGK0Bi449zJWuz0tHfVoTFUOKVvxdkEvnoIoQ";
static const char* RSA_OTHER_N = "uHR78K6Zw0tIHEt2-10YqJvNqj-hNmESDfuEYIe5cQ9BQxnTzWj-SbQ0ObrI3rwl0S0jMwx027e-PuUH_qWeq5dL1HwYj2sTFDle7zHgwYTFzY7ujgxiLeBBMo8jcW2KNPOogb8E9al1JgkE_Q3OImEQT0uF2-WytvnSfEhux7TyLQp-FjuTPU8mfm0oreIoS6zPbDLJWj5eWH0bu_mIpYMNEA8lY9gUKWyTKX0-oT_HfEyeyaCaeVT17NtNmpoF7AL1u9PpVH8U5dcUrCtM-1npBBFqk6qeYQnZhgBBSYlas5VOE9HZfOzj5ucvJwt_3zm_08qeBakXbJ6T667lSQ";
static const char* RSA_GOOD = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMTExMTExMS0yMjIyLTMzMzMtNDQ0NC01NTU1NTU1NTU1NTUiLCJqdGkiOiJhYWFhYWFhYS1iYmJiLWNjY2MtZGRkZC1lZWVlZWVlZWVlZWUiLCJhdWQiOiIwNGExYjJjM2Q0ZTVmNiIsImlhdCI6MTcwMDAwMDAwMCwibmJmIjoxNzAwMDAwMTAwLCJleHAiOjE4MDAwMDAwMDB9.JIxMsTzVzPZEEauQSXUyt_VrvoTy6MDJ_UmN41Vlkifa12fMIucZ5MyaER9Odm3q0UPTIIsj9uBRB9WB2QxuLkitejirK1IYafHvLafYhUcYv79AusViyAN_7VuckN2uL705VC7eftWZC2K6rMnwyitWro1MbJd-llxzOuULseousm-1rADLFz-JuwW-7g8VIm71OGD3IRFhj6xg1Nh11wMIGSqBpQfWvZOkd4mqbx93s7D7skxSR7V-2oCuNQuUOqQUJu50J9EGyQ3vgpAqtPLRcW7NATWw2G5J2Sw9T9yvD87ZJfzMwKofGNwuCcyR0f9yQaiZJYGEOXrI6o6xCQ";
static const char* RSA_OTHER = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMTExMTExMS0yMjIyLTMzMzMtNDQ0NC01NTU1NTU1NTU1NTUiLCJqdGkiOiJhYWFhYWFhYS1iYmJiLWNjY2MtZGRkZC1lZWVlZWVlZWVlZWUiLCJhdWQiOiIwNGExYjJjM2Q0ZTVmNiIsImlhdCI6MTcwMDAwMDAwMCwibmJmIjoxNzAwMDAwMTAwLCJleHAiOjE4MDAwMDAwMDB9.GJjToRmh4G-QRMB6nFVVPkvUB5FLbNCWPmz-xm5VxxccN6cIxT-DTG-xk6bfu-xoEJ8t3y8CtmlSOeW30KHez7CObflh28Q3qRlbxY_QLyKoVFRrrdN_1N_CdK6t0FWn7wqrjgS9PGw_l92EMAiJPrVC9_jgrT4kp8PJh7S6F7WNFuYD1t4Hta_glL9ZIQx8aK9EANa-YUMa18EFGAiDh23HOJ2iBPDSOS33esgSmc5xE--7N2yjqJ9GVYCC0fjfhqf4RaeA092RRxchZjsrocYiRMQq7T4FfW3nN5EBtu0BQunWYxoQ1O5j-IBcGhfbtPEbxHfJ3FOzzVs-pDNfgQ";

static void testRsaJwt() {
	uint8_t n[kRsaBytes + 1], other[kRsaBytes + 1];
	CHECK(base64UrlDecode(RSA_N, strlen(RSA_N), n, sizeof(n)) == (int)kRsaBytes);
	CHECK(base64UrlDecode(RSA_OTHER_N, strlen(RSA_OTHER_N), other, sizeof(other)) == (int)kRsaBytes);
	VerifyKeys keys{(const uint8_t*)SECRET, strlen(SECRET), n};
	KeyClaims k;
	CHECK(jwtVerify(RSA_GOOD, strlen(RSA_GOOD), keys, k) == JwtResult::Ok);
	CHECK(!strcmp(k.jti, "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee") && k.audLen == 7 && k.aud[0] == 0x04 && k.aud[6] == 0xf6);
	CHECK(k.exp == 1800000000u && k.nbf == 1700000100u);
	// Signed by another key pair, or checked against another public key.
	CHECK(jwtVerify(RSA_OTHER, strlen(RSA_OTHER), keys, k) == JwtResult::BadSignature);
	VerifyKeys wrong{(const uint8_t*)SECRET, strlen(SECRET), other};
	CHECK(jwtVerify(RSA_GOOD, strlen(RSA_GOOD), wrong, k) == JwtResult::BadSignature);
	// No public key known yet.
	VerifyKeys none{(const uint8_t*)SECRET, strlen(SECRET), nullptr};
	CHECK(jwtVerify(RSA_GOOD, strlen(RSA_GOOD), none, k) == JwtResult::NoPublicKey);
	// A changed claim, last signature character, or a truncated signature must not verify.
	std::string t = RSA_GOOD;
	t[t.find('.') + 5] ^= 1;
	CHECK(jwtVerify(t.c_str(), t.size(), keys, k) != JwtResult::Ok);
	t = RSA_GOOD;
	t[t.size() - 3] = t[t.size() - 3] == 'A' ? 'B' : 'A';
	CHECK(jwtVerify(t.c_str(), t.size(), keys, k) == JwtResult::BadSignature);
	t = RSA_GOOD;
	t.pop_back();
	CHECK(jwtVerify(t.c_str(), t.size(), keys, k) == JwtResult::Malformed);
	// An RS256 token never verifies with the shared secret, and an HS256 token never with the public key.
	CHECK(jwtVerify(GOOD, strlen(GOOD), VerifyKeys{(const uint8_t*)"x", 1, n}, k) == JwtResult::BadSignature);
	CHECK(jwtVerify(GOOD, strlen(GOOD), hsKeys(), k) == JwtResult::Ok);
	// A modulus that is not 2048 bits, or even, is refused.
	uint8_t bad[kRsaBytes];
	memcpy(bad, n, sizeof(bad));
	bad[kRsaBytes - 1] &= 0xfe;
	CHECK(jwtVerify(RSA_GOOD, strlen(RSA_GOOD), VerifyKeys{nullptr, 0, bad}, k) == JwtResult::BadSignature);
}

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
	CHECK(jwtVerify(GOOD, strlen(GOOD), hsKeys(), k) == JwtResult::Ok);
	CHECK(!strcmp(k.jti, "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"));
	CHECK(!strcmp(k.sub, "11111111-2222-3333-4444-555555555555"));
	CHECK(k.audLen == 7 && k.aud[0] == 0x04 && k.aud[6] == 0xf6);
	CHECK(k.iat == 1700000000u && k.nbf == 1700000100u && k.exp == 1800000000u);

	CHECK(jwtVerify(WRONG_KEY, strlen(WRONG_KEY), hsKeys(), k) == JwtResult::BadSignature);
	CHECK(jwtVerify(ALG_NONE, strlen(ALG_NONE), hsKeys(), k) == JwtResult::BadClaims);  // right MAC, wrong alg
	CHECK(jwtVerify(OLD_UID, strlen(OLD_UID), hsKeys(), k) == JwtResult::Ok && k.audLen == 4 &&
	      k.aud[1] == 0xa1);

	// Tampering with one payload character must break the signature.
	char tampered[700];
	strcpy(tampered, GOOD);
	tampered[60] = tampered[60] == 'A' ? 'B' : 'A';
	CHECK(jwtVerify(tampered, strlen(tampered), hsKeys(), k) != JwtResult::Ok);
	CHECK(jwtVerify("", 0, hsKeys(), k) == JwtResult::Malformed);
	CHECK(jwtVerify("a.b", 3, hsKeys(), k) == JwtResult::Malformed);
	CHECK(jwtVerify("aaaaaaaaaaaaaaaaaaaaaaaa.bbbbbbbbbbbb", 37, hsKeys(), k) == JwtResult::Malformed);
	char huge[2000];
	memset(huge, 'a', sizeof(huge) - 1);
	huge[sizeof(huge) - 1] = 0;
	CHECK(jwtVerify(huge, strlen(huge), hsKeys(), k) == JwtResult::Malformed);

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

	// What the door writes reads back as the same token: short and long records, and a refusal when it cannot fit.
	for (const char* t : {GOOD, (const char*)big}) {
		uint8_t enc[1024];
		size_t n = ndefEncodeToken(t, strlen(t), enc, sizeof(enc));
		CHECK(n > strlen(t) && enc[n - 1] == 0xFE);
		memset(area, 0, sizeof(area));
		memcpy(area, enc, n);
		tag = {n, 0};
		CHECK(ndefFindToken(area, fakeFill, &tag, tok, len) && len == strlen(t) && !memcmp(tok, t, len));
		CHECK(ndefEncodeToken(t, strlen(t), enc, n - 1) == 0);
	}

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

// An unmasked frame as a server sends it.
static std::vector<uint8_t> serverFrame(bool fin, uint8_t op, const std::string& payload) {
	std::vector<uint8_t> f = {(uint8_t)((fin ? 0x80 : 0) | op)};
	size_t n = payload.size();
	if (n < 126) f.push_back((uint8_t)n);
	else { f.push_back(126); f.push_back((uint8_t)(n >> 8)); f.push_back((uint8_t)n); }
	f.insert(f.end(), payload.begin(), payload.end());
	return f;
}

using WsEvent = ws::Parser::Event;

// Feeds everything, collecting message texts and the other events in order.
static std::vector<std::string> wsRun(ws::Parser& p, const std::vector<uint8_t>& bytes, size_t step = 0) {
	std::vector<std::string> log;
	size_t at = 0;
	while (at < bytes.size()) {
		size_t n = step ? std::min(step, bytes.size() - at) : bytes.size() - at;
		size_t used = 0;
		WsEvent e = p.feed(bytes.data() + at, n, &used);
		at += used;
		switch (e) {
			case WsEvent::None: break;
			case WsEvent::Message: log.push_back("M"); break;
			case WsEvent::Dropped: log.push_back("D"); break;
			case WsEvent::Ping: log.push_back("P:" + std::string((const char*)p.control(), p.controlLength())); break;
			case WsEvent::Pong: log.push_back("O"); break;
			case WsEvent::Close: log.push_back("C"); break;
			case WsEvent::Error: log.push_back("E"); return log;
		}
	}
	return log;
}

static void testWsBuild() {
	const uint8_t key[4] = {0x01, 0x02, 0x03, 0x04};
	uint8_t h[ws::kMaxHeader];
	CHECK(ws::buildHeader(h, ws::kText, 5, key) == 6);
	CHECK(h[0] == 0x81 && h[1] == 0x85 && !memcmp(h + 2, key, 4));
	CHECK(ws::buildHeader(h, ws::kText, 300, key) == 8);
	CHECK(h[0] == 0x81 && h[1] == 0xFE && h[2] == 0x01 && h[3] == 0x2C && !memcmp(h + 4, key, 4));
	CHECK(ws::buildHeader(h, ws::kPong, 0, key) == 6 && h[0] == 0x8A && h[1] == 0x80);
	CHECK(ws::buildHeader(h, ws::kText, 65535, key) == 8);
	CHECK(ws::buildHeader(h, ws::kText, 65536, key) == 0);

	uint8_t data[7] = {'p', 'a', 'y', 'l', 'o', 'a', 'd'};
	ws::mask(data, sizeof(data), key);
	CHECK(data[0] == ('p' ^ 1) && data[4] == ('o' ^ 1) && data[3] == ('l' ^ 4));
	ws::mask(data, sizeof(data), key);
	CHECK(!memcmp(data, "payload", 7));
}

static void testWsParse() {
	uint8_t buf[64];
	ws::Parser p;

	// One message, whole and one byte at a time.
	for (size_t step : {(size_t)0, (size_t)1, (size_t)3}) {
		p.reset(buf, sizeof(buf));
		auto log = wsRun(p, serverFrame(true, 0x1, "hello"), step);
		CHECK(log.size() == 1 && log[0] == "M" && p.length() == 5 && !strcmp((const char*)buf, "hello"));
	}

	// The extended 16-bit length.
	{
		uint8_t big[400];
		ws::Parser q;
		q.reset(big, sizeof(big));
		std::string text(200, 'x');
		auto log = wsRun(q, serverFrame(true, 0x1, text), 7);
		CHECK(log.size() == 1 && log[0] == "M" && q.length() == 200 && big[199] == 'x' && big[200] == 0);
	}

	// Two messages in one read: the first call stops after the first.
	{
		p.reset(buf, sizeof(buf));
		std::vector<uint8_t> two = serverFrame(true, 0x1, "one");
		std::vector<uint8_t> second = serverFrame(true, 0x1, "second");
		two.insert(two.end(), second.begin(), second.end());
		size_t used = 0;
		CHECK(p.feed(two.data(), two.size(), &used) == WsEvent::Message && used == 5 && !strcmp((const char*)buf, "one"));
		CHECK(p.feed(two.data() + used, two.size() - used, &used) == WsEvent::Message && !strcmp((const char*)buf, "second"));
	}

	// Empty message.
	p.reset(buf, sizeof(buf));
	{
		size_t used = 0;
		auto f = serverFrame(true, 0x1, "");
		CHECK(p.feed(f.data(), f.size(), &used) == WsEvent::Message && p.length() == 0 && buf[0] == 0 && used == 2);
	}

	// A fragmented message with a ping in the middle: the ping is answered, the message survives.
	{
		p.reset(buf, sizeof(buf));
		std::vector<uint8_t> all = serverFrame(false, 0x1, "hel");
		for (auto& part : {serverFrame(true, 0x9, "p"), serverFrame(true, 0x0, "lo")}) all.insert(all.end(), part.begin(), part.end());
		auto log = wsRun(p, all, 2);
		CHECK(log.size() == 2 && log[0] == "P:p" && log[1] == "M" && !strcmp((const char*)buf, "hello"));
	}

	// Pong, close.
	{
		p.reset(buf, sizeof(buf));
		auto f = serverFrame(true, 0xA, "");
		auto c = serverFrame(true, 0x8, std::string("\x03\xE8", 2));
		f.insert(f.end(), c.begin(), c.end());
		auto log = wsRun(p, f);
		CHECK(log.size() == 2 && log[0] == "O" && log[1] == "C" && p.controlLength() == 2 && p.control()[0] == 0x03);
	}

	// A message larger than the buffer is skipped, and the next one still arrives.
	{
		uint8_t small[16];
		ws::Parser q;
		q.reset(small, sizeof(small));
		std::vector<uint8_t> all = serverFrame(true, 0x1, std::string(40, 'z'));
		auto next = serverFrame(true, 0x1, "ok");
		all.insert(all.end(), next.begin(), next.end());
		auto log = wsRun(q, all, 5);
		CHECK(log.size() == 2 && log[0] == "D" && log[1] == "M" && !strcmp((const char*)small, "ok"));
		// 15 bytes fit (the 16th holds the terminator), 16 do not.
		q.reset(small, sizeof(small));
		CHECK(wsRun(q, serverFrame(true, 0x1, std::string(15, 'a')))[0] == "M");
		q.reset(small, sizeof(small));
		CHECK(wsRun(q, serverFrame(true, 0x1, std::string(16, 'a')))[0] == "D");
	}

	// Protocol violations end in an error that stays.
	{
		size_t used;
		p.reset(buf, sizeof(buf));
		auto masked = serverFrame(true, 0x1, "x");
		masked[1] |= 0x80;
		CHECK(p.feed(masked.data(), masked.size(), &used) == WsEvent::Error);
		auto fine = serverFrame(true, 0x1, "x");
		CHECK(p.feed(fine.data(), fine.size(), &used) == WsEvent::Error);  // sticky

		p.reset(buf, sizeof(buf));
		auto rsv = serverFrame(true, 0x1, "x");
		rsv[0] |= 0x40;
		CHECK(p.feed(rsv.data(), rsv.size(), &used) == WsEvent::Error);

		p.reset(buf, sizeof(buf));
		auto orphan = serverFrame(true, 0x0, "x");  // continuation of nothing
		CHECK(p.feed(orphan.data(), orphan.size(), &used) == WsEvent::Error);

		p.reset(buf, sizeof(buf));
		auto unfinished = serverFrame(false, 0x1, "a");
		auto again = serverFrame(true, 0x1, "b");  // new message while one is open
		unfinished.insert(unfinished.end(), again.begin(), again.end());
		CHECK(wsRun(p, unfinished).back() == "E");

		p.reset(buf, sizeof(buf));
		auto fragmentedControl = serverFrame(false, 0x9, "x");
		CHECK(p.feed(fragmentedControl.data(), fragmentedControl.size(), &used) == WsEvent::Error);

		p.reset(buf, sizeof(buf));
		auto longControl = serverFrame(true, 0x9, std::string(126, 'x'));
		CHECK(p.feed(longControl.data(), longControl.size(), &used) == WsEvent::Error);

		p.reset(buf, sizeof(buf));
		auto reservedOp = serverFrame(true, 0x3, "x");
		CHECK(p.feed(reservedOp.data(), reservedOp.size(), &used) == WsEvent::Error);

		p.reset(buf, sizeof(buf));
		const uint8_t huge[] = {0x81, 127, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
		CHECK(p.feed(huge, sizeof(huge), &used) == WsEvent::Error);
	}
}

static void testBackoff() {
	Backoff b(1000, 8000);
	CHECK(b.due(0) && b.due(123456));  // never failed: always due
	CHECK(b.fail(100, 0) == 1000 && !b.due(1099) && b.due(1100));
	CHECK(b.fail(2000, 0) == 2000);
	CHECK(b.fail(5000, 0) == 4000);
	CHECK(b.fail(10000, 0) == 8000);
	CHECK(b.fail(20000, 0) == 8000 && b.failures() == 5);  // capped
	// Jitter stays within a quarter on top of the delay.
	Backoff j(1000, 8000);
	uint32_t w = j.fail(0, 0xFFFFFFFFu);
	CHECK(w >= 1000 && w <= 1250);
	b.succeed();
	CHECK(b.due(0) && b.failures() == 0 && b.fail(0, 0) == 1000);
	// A fixed hold, across the 32-bit wrap of millis().
	Backoff h(1000, 8000);
	h.hold(0xFFFFFF00u, 1000);
	CHECK(!h.due(0xFFFFFF00u) && !h.due(0xFFFFFFFFu) && !h.due(0x000002E7u) && h.due(0x000002E8u) && h.due(5000));
}

static void testAccessToken() {
	AccessToken t;
	char out[64];
	CHECK(!t.present() && !t.copyTo(out, sizeof(out)));
	uint32_t g0 = t.generation();
	CHECK(t.set("aaa.bbb.ccc") && t.present() && t.generation() != g0);
	uint32_t g = 0;
	CHECK(t.copyTo(out, sizeof(out), &g) && !strcmp(out, "aaa.bbb.ccc") && g == t.generation());
	CHECK(!t.copyTo(out, 11));   // needs room for the NUL
	CHECK(t.copyTo(out, 12));
	uint32_t g1 = t.generation();
	CHECK(t.set("ddd") && t.generation() != g1 && t.copyTo(out, sizeof(out)) && !strcmp(out, "ddd"));
	std::string tooLong(AccessToken::kMax, 'x');
	CHECK(!t.set(tooLong.c_str()) && !t.set("") && t.copyTo(out, sizeof(out)) && !strcmp(out, "ddd"));  // unchanged
	std::string longest(AccessToken::kMax - 1, 'y');
	CHECK(t.set(longest.c_str()));
	uint32_t g2 = t.generation();
	t.clear();
	CHECK(!t.present() && t.generation() != g2 && !t.copyTo(out, sizeof(out)));
}

static void testBlacklistReplace() {
	BlacklistSet a, b;
	a.upsert("old1", 2000);
	a.upsert("old2", 2000);
	a.upsert("shared", 2000);
	b.upsert("shared", 3000);
	b.upsert("new1", 3000);
	a.replaceWith(b);
	CHECK(a.size() == 2 && a.contains("shared") && a.contains("new1") && !a.contains("old1") && !a.contains("old2"));
	CHECK(b.size() == 2);  // the source is untouched
	a.replaceWith(a);
	CHECK(a.size() == 2 && a.contains("new1"));
	// The copy is independent of its source.
	b.remove("new1");
	CHECK(a.contains("new1") && !b.contains("new1"));
	// An empty source empties the target, and the sorted order survives for later inserts.
	BlacklistSet empty;
	a.replaceWith(empty);
	CHECK(a.size() == 0 && !a.contains("shared"));
	a.upsert("later", 5);
	CHECK(a.contains("later") && a.size() == 1);
}

static void testChimes() {
	// 130 bpm sixteenths, rolling C E G C G E; a short silence ends each step.
	CHECK(Chimes::unlocked(0) == 880 && Chimes::unlocked(100) == 1175 && Chimes::unlocked(200) == 0);
	CHECK(Chimes::unlocked(300) == 880);  // loops

	CHECK(Chimes::denied(0) == 659 && Chimes::denied(150) == 523 && Chimes::denied(300) == 440);
	CHECK(Chimes::denied(450) == 220 && Chimes::denied(Chimes::kDeniedMs - 1) == 220);
	CHECK(Chimes::denied(Chimes::kDeniedMs) == 0 && Chimes::denied(100000) == 0);

	// The countdown alert repeats for the whole delay.
	CHECK(Chimes::attention(0) == 880 && Chimes::attention(200) == 1175 && Chimes::attention(400) == 0);
	CHECK(Chimes::attention(600) == 880);
	CHECK(Chimes::kForceOpenDelayMs == 3000);
}

int main() {
	testChimes();
	testWsBuild();
	testWsParse();
	testBackoff();
	testAccessToken();
	testBlacklistReplace();
	testNdef();
	testPn532Frame();
	testHash();
	testJwt();
	testRsaJwt();
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
