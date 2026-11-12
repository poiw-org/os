#include "Access.h"
#include <string.h>
#include "P256.h"

// The rules every key shares, whatever carries it.
static Verdict checkKey(const KeyClaims& key, uint32_t now, bool clockTrusted, bool blacklisted) {
	if (blacklisted) return Verdict::Blacklisted;
	if (now >= key.exp) return Verdict::Expired;
	if (now + kNotBeforeLeeway < key.nbf) return Verdict::NotYetActive;
	if (clockTrusted && key.iat > now + kIssuedAtLeeway) return Verdict::IssuedInFuture;
	return Verdict::Allow;
}

Verdict decide(const KeyClaims& key, const uint8_t* uid, size_t uidLen, uint32_t now, bool clockTrusted,
               bool blacklisted) {
	if (key.audience != Audience::Tag || uidLen != key.audLen || memcmp(uid, key.aud, uidLen) != 0)
		return Verdict::SerialMismatch;
	return checkKey(key, now, clockTrusted, blacklisted);
}

BleResult decideBle(const KeyClaims& key, const char* doorId, const uint8_t nonce[kBleNonceLen],
                    const uint8_t sig[kBleSigLen], uint32_t now, bool clockTrusted, bool blacklisted) {
	if (key.audience != Audience::Ble || !p256ValidPoint(key.cnf)) return BleResult::BadCredential;
	uint8_t hash[32];
	bleProofHash(doorId, nonce, hash);
	if (!p256Verify(key.cnf, hash, sig)) return BleResult::ProofFailed;
	switch (checkKey(key, now, clockTrusted, blacklisted)) {
		case Verdict::Allow: return BleResult::Opened;
		case Verdict::Blacklisted: return BleResult::Disabled;
		case Verdict::Expired: return BleResult::Expired;
		case Verdict::NotYetActive: return BleResult::NotYetValid;
		default: return BleResult::BadCredential;  // issued in the future
	}
}

uint8_t journalVerdict(BleResult r) {
	switch (r) {
		case BleResult::Disabled: return (uint8_t)Verdict::Blacklisted;
		case BleResult::Expired: return (uint8_t)Verdict::Expired;
		case BleResult::NotYetValid: return (uint8_t)Verdict::NotYetActive;
		default: return 255;
	}
}

const char* reasonName(Verdict v) {
	switch (v) {
		case Verdict::SerialMismatch: return "SerialNumberMismatch";
		case Verdict::Blacklisted: return "KeyBlacklisted";
		case Verdict::Expired: return "KeyExpired";
		case Verdict::NotYetActive: return "KeyNotYetActivated";
		case Verdict::IssuedInFuture: return "IssuingDateInTheFuture";
		default: return nullptr;
	}
}
