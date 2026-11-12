#include "BridgeLink.h"
#include <string.h>
#include "Crc.h"

namespace Link {

bool knownType(uint8_t t) {
	switch (t) {
		case kHello: case kPing: case kPong: case kConnected:
		case kChallenge: case kRequest: case kResult: case kGone:
			return true;
		default:
			return false;
	}
}

// COBS: the data is cut into runs of at most 254 non-zero bytes, each preceded by a code byte that
// says how far the next zero is, so the encoded text never contains 0x00 and 0x00 can end a frame.
static size_t cobsEncode(const uint8_t* in, size_t len, uint8_t* out, size_t cap) {
	size_t w = 1, code = 0;  // out[0] is the first code byte
	if (cap < 1) return 0;
	for (size_t i = 0; i < len; i++) {
		if (in[i] == 0) {
			out[code] = (uint8_t)(w - code);
			code = w++;
			if (w > cap) return 0;
		} else {
			if (w >= cap) return 0;
			out[w++] = in[i];
			if (w - code == 0xFF) {
				out[code] = 0xFF;
				code = w++;
				if (w > cap) return 0;
			}
		}
	}
	out[code] = (uint8_t)(w - code);
	return w;
}

// Decodes in place (the output is never longer than the input). Returns the length or -1.
static int cobsDecode(uint8_t* buf, size_t len) {
	size_t r = 0, w = 0;
	while (r < len) {
		uint8_t code = buf[r++];
		if (code == 0) return -1;
		size_t run = (size_t)code - 1;
		if (r + run > len) return -1;
		for (size_t i = 0; i < run; i++) buf[w++] = buf[r++];
		if (code != 0xFF && r < len) buf[w++] = 0;
	}
	return (int)w;
}

size_t encode(uint8_t type, uint8_t conn, const uint8_t* payload, size_t len, uint8_t* out, size_t cap) {
	if (len > kMaxPayload) return 0;
	uint8_t raw[kMaxDecoded];
	raw[0] = type;
	raw[1] = conn;
	if (len) memcpy(raw + 2, payload, len);
	uint16_t crc = crc16(raw, 2 + len);
	raw[2 + len] = (uint8_t)(crc >> 8);
	raw[3 + len] = (uint8_t)crc;
	if (cap < 1) return 0;
	size_t n = cobsEncode(raw, 4 + len, out, cap - 1);
	if (!n) return 0;
	out[n] = 0;
	return n + 1;
}

bool Decoder::feed(uint8_t byte) {
	if (byte != 0) {
		if (n_ < sizeof(buf_)) buf_[n_++] = byte;
		else overflow_ = true;
		return false;
	}
	// End of a frame (or of noise).
	size_t len = n_;
	bool bad = overflow_;
	n_ = 0;
	overflow_ = false;
	if (len == 0 && !bad) return false;  // an empty frame carries nothing and is not an error
	if (bad) { dropped_++; return false; }

	int d = cobsDecode(buf_, len);
	if (d < 4 || (size_t)d > kMaxDecoded) { dropped_++; return false; }
	size_t body = (size_t)d - 2;
	uint16_t crc = (uint16_t)(buf_[body] << 8 | buf_[body + 1]);
	if (crc != crc16(buf_, body) || !knownType(buf_[0]) || body - 2 > kMaxPayload) {
		dropped_++;
		return false;
	}
	frame_.type = buf_[0];
	frame_.conn = buf_[1];
	frame_.payload = buf_ + 2;
	frame_.len = body - 2;
	return true;
}

}  // namespace Link
