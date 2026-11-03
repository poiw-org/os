#include "Access.h"
#include <string.h>

Verdict decide(const KeyClaims& key, const uint8_t* uid, size_t uidLen, uint32_t now, bool clockTrusted,
               bool blacklisted) {
	if (uidLen != key.audLen || memcmp(uid, key.aud, uidLen) != 0) return Verdict::SerialMismatch;
	if (blacklisted) return Verdict::Blacklisted;
	if (now >= key.exp) return Verdict::Expired;
	if (now + kNotBeforeLeeway < key.nbf) return Verdict::NotYetActive;
	if (clockTrusted && key.iat > now + kIssuedAtLeeway) return Verdict::IssuedInFuture;
	return Verdict::Allow;
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
