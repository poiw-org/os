#include "P256.h"
extern "C" {
#include "uECC.h"
}

bool p256ValidPoint(const uint8_t pub[65]) {
	// micro-ecc takes X || Y without the 0x04 tag, so the tag is checked here.
	return pub[0] == 0x04 && uECC_valid_public_key(pub + 1, uECC_secp256r1()) != 0;
}

bool p256Verify(const uint8_t pub[65], const uint8_t hash[32], const uint8_t sig[64]) {
	if (!p256ValidPoint(pub)) return false;
	return uECC_verify(pub + 1, hash, 32, sig, uECC_secp256r1()) != 0;
}
