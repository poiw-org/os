#include "T4t.h"
#include <string.h>

namespace {
constexpr uint8_t kAid[7] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};  // NDEF Tag Application
constexpr uint16_t kCcId = 0xE103;
constexpr uint16_t kNdefId = 0xE104;

size_t status(uint8_t* out, size_t i, uint16_t sw) {
	out[i++] = (uint8_t)(sw >> 8);
	out[i++] = (uint8_t)sw;
	return i;
}

// Capability Container: version 2.0, MLe, MLc, then the NDEF file control TLV (read-only).
void capabilities(const T4tTag& tag, uint8_t cc[15]) {
	const uint8_t fixed[15] = {0x00, 0x0F, 0x20, 0x00, (uint8_t)kT4tMaxRead, 0x00, 0x01, 0x04, 0x06,
	                           (uint8_t)(kNdefId >> 8), (uint8_t)kNdefId, 0x00, 0x00, 0x00, 0xFF};
	memcpy(cc, fixed, 15);
	cc[11] = (uint8_t)(sizeof(tag.ndef) >> 8);
	cc[12] = (uint8_t)sizeof(tag.ndef);
}
}  // namespace

size_t t4tRespond(T4tTag& tag, const uint8_t* a, size_t n, uint8_t* out, size_t cap) {
	if (n < 4 || cap < kT4tMaxRead + 2) return status(out, 0, 0x6700);
	uint8_t ins = a[1], p1 = a[2], p2 = a[3];
	if (a[0] != 0x00) return status(out, 0, 0x6E00);

	if (ins == 0xA4) {  // SELECT
		if (n < 5 || n < 5u + a[4]) return status(out, 0, 0x6700);
		const uint8_t* data = a + 5;
		size_t lc = a[4];
		if (p1 == 0x04) {  // by name: the NDEF application
			tag.file = T4tTag::File::None;
			tag.appSelected = lc == sizeof(kAid) && memcmp(data, kAid, sizeof(kAid)) == 0;
			return status(out, 0, tag.appSelected ? 0x9000 : 0x6A82);
		}
		if (p1 == 0x00 && p2 == 0x0C && lc == 2 && tag.appSelected) {  // by file id
			uint16_t id = (uint16_t)(data[0] << 8 | data[1]);
			tag.file = id == kCcId ? T4tTag::File::Cc : id == kNdefId ? T4tTag::File::Ndef : T4tTag::File::None;
			return status(out, 0, tag.file != T4tTag::File::None ? 0x9000 : 0x6A82);
		}
		return status(out, 0, 0x6A82);
	}

	if (ins == 0xB0) {  // READ BINARY
		uint8_t cc[15];
		const uint8_t* file;
		size_t size;
		if (tag.file == T4tTag::File::Cc) {
			capabilities(tag, cc);
			file = cc;
			size = sizeof(cc);
		} else if (tag.file == T4tTag::File::Ndef) {
			file = tag.ndef;
			size = tag.ndefLen;
		} else {
			return status(out, 0, 0x6986);
		}
		size_t offset = (size_t)p1 << 8 | p2;
		if (offset > size) return status(out, 0, 0x6B00);
		size_t le = n > 4 && a[4] ? a[4] : 256;
		size_t count = size - offset;
		if (count > le) count = le;
		if (count > kT4tMaxRead) count = kT4tMaxRead;
		memcpy(out, file + offset, count);
		if (tag.file == T4tTag::File::Ndef && offset + count >= size && size > 2) tag.served = true;
		return status(out, count, 0x9000);
	}

	return status(out, 0, ins == 0xD6 ? 0x6982 : 0x6D00);  // UPDATE BINARY: read-only
}
