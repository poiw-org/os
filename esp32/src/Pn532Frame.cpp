#include "Pn532Frame.h"

namespace pn532 {

size_t buildFrame(uint8_t* out, const uint8_t* payload, size_t n) {
	uint8_t len = (uint8_t)(n + 1);  // TFI + payload
	out[0] = 0x00;
	out[1] = 0x00;
	out[2] = 0xFF;
	out[3] = len;
	out[4] = (uint8_t)(0x100 - len);
	out[5] = kTfiToChip;
	uint8_t sum = kTfiToChip;
	for (size_t i = 0; i < n; i++) {
		out[6 + i] = payload[i];
		sum = (uint8_t)(sum + payload[i]);
	}
	out[6 + n] = (uint8_t)(0x100 - sum);
	out[7 + n] = 0x00;
	return n + 8;
}

bool headerValid(const uint8_t h[5]) {
	return h[0] == 0x00 && h[1] == 0x00 && h[2] == 0xFF && (uint8_t)(h[3] + h[4]) == 0;
}

}  // namespace pn532
