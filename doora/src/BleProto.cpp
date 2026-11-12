#include "BleProto.h"
#include <string.h>
#include "Sha256.h"

size_t bleEncodeChallenge(const uint8_t nonce[kBleNonceLen], const char* doorId, uint8_t* out, size_t cap) {
	size_t id = strlen(doorId);
	if (id == 0 || id > kBleMaxDoorId || cap < 3 + kBleNonceLen + id) return 0;
	out[0] = kBleChallenge;
	out[1] = kBleVersion;
	memcpy(out + 2, nonce, kBleNonceLen);
	out[2 + kBleNonceLen] = (uint8_t)id;
	memcpy(out + 3 + kBleNonceLen, doorId, id);
	return 3 + kBleNonceLen + id;
}

bool bleParseRequest(const uint8_t* body, size_t len, BleRequest& out) {
	if (len < 2 + kBleSigLen + 1 || len > kBleMaxBody) return false;
	if (body[0] != kBleRequest || body[1] != kBleVersion) return false;
	out.sig = body + 2;
	out.jwt = (const char*)body + 2 + kBleSigLen;
	out.jwtLen = len - 2 - kBleSigLen;
	return true;
}

void bleProofHash(const char* doorId, const uint8_t nonce[kBleNonceLen], uint8_t hash[32]) {
	static const char kContext[] = "doora-ble-1";
	size_t id = strlen(doorId);
	Sha256 sha;
	sha.update((const uint8_t*)kContext, sizeof(kContext) - 1);
	uint8_t idLen = (uint8_t)id;
	sha.update(&idLen, 1);
	sha.update((const uint8_t*)doorId, id);
	sha.update(nonce, kBleNonceLen);
	sha.final(hash);
}
