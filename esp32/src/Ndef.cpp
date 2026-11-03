#include "Ndef.h"

namespace {

// Payload of the first record of an NDEF message (NFC Forum NDEF 3.2), or nullptr when malformed.
const uint8_t* firstRecordPayload(const uint8_t* m, size_t len, size_t& payloadLen) {
	if (len < 3) return nullptr;
	uint8_t flags = m[0];
	size_t typeLen = m[1];
	size_t i = 2;
	if (flags & 0x10) {  // SR: 1-byte payload length
		payloadLen = m[i++];
	} else {
		if (len < 6) return nullptr;
		payloadLen = (size_t)m[2] << 24 | (size_t)m[3] << 16 | (size_t)m[4] << 8 | m[5];
		i = 6;
	}
	size_t idLen = 0;
	if (flags & 0x08) {  // IL: id length byte
		if (i >= len) return nullptr;
		idLen = m[i++];
	}
	i += typeLen + idLen;
	if (i > len || payloadLen > len - i) return nullptr;
	return m + i;
}

// The token sits after a text record's status byte and language code (normally 3 bytes). Locate it
// by its fixed JWT header prefix instead of trusting that layout.
bool locateToken(const uint8_t* p, size_t len, const uint8_t*& token, size_t& tokenLen) {
	for (size_t i = 0; i + 3 <= len && i <= 8; i++) {
		if (p[i] == 'e' && p[i + 1] == 'y' && p[i + 2] == 'J') {
			token = p + i;
			tokenLen = len - i;
			return true;
		}
	}
	return false;
}

}  // namespace

bool ndefFindToken(const uint8_t* area, NdefFill fill, void* ctx, const uint8_t*& token, size_t& len) {
	size_t i = 0;
	for (;;) {
		if (!fill(ctx, i + 1)) return false;
		uint8_t tag = area[i];
		if (tag == 0x00) { i++; continue; }  // padding
		if (tag == 0xFE) return false;       // terminator: no NDEF message
		if (!fill(ctx, i + 2)) return false;
		size_t size = area[i + 1];
		size_t header = 2;
		if (size == 0xFF) {  // 3-byte length form, for messages of 255 bytes or more
			if (!fill(ctx, i + 4)) return false;
			size = (size_t)area[i + 2] << 8 | area[i + 3];
			header = 4;
		}
		if (tag != 0x03) { i += header + size; continue; }  // lock or memory control TLV
		if (!fill(ctx, i + header + size)) return false;
		size_t payloadLen;
		const uint8_t* payload = firstRecordPayload(area + i + header, size, payloadLen);
		return payload && locateToken(payload, payloadLen, token, len);
	}
}
