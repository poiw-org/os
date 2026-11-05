#include "OtaImage.h"
#include <string.h>
#include "Sha256.h"

bool otaImageSigned(const RsaPublicKey& key, OtaRead read, void* ctx, size_t total) {
	if (total <= kOtaTrailer) return false;
	size_t imageLen = total - kOtaTrailer;
	uint8_t trailer[kOtaTrailer];
	if (!read(ctx, imageLen, trailer, sizeof(trailer))) return false;
	if (memcmp(trailer + kRsaBytes, kOtaMagic, sizeof(kOtaMagic)) != 0) return false;

	Sha256 sha;
	uint8_t chunk[512];
	for (size_t at = 0; at < imageLen;) {
		size_t n = imageLen - at < sizeof(chunk) ? imageLen - at : sizeof(chunk);
		if (!read(ctx, at, chunk, n)) return false;
		sha.update(chunk, n);
		at += n;
	}
	uint8_t hash[32];
	sha.final(hash);
	return rsaVerifySha256(key, trailer, hash);
}
