#include "Reader.h"
#include <string.h>
#include "Log.h"

namespace {
// How long one presence check waits. Short, so the websocket and relay timers stay responsive.
constexpr uint32_t kPollMs = 60;
// A tag must be gone this long before it can be read again.
constexpr uint32_t kReleaseMs = 600;
constexpr uint32_t kHealthEveryMs = 300000;
constexpr uint32_t kRetryEveryMs = 10000;
constexpr uint8_t kPn532Addr = 0x48 >> 1;
constexpr size_t kMaxPayload = kMaxTokenLen + 8;
uint8_t gPayload[kMaxPayload];
// Type 2 tag data area (from page 4), large enough for the NDEF TLV holding a maximum-size token.
constexpr size_t kMaxArea = 896;
constexpr uint8_t kLastPage = 230;  // NTAG216 user memory ends at page 225
uint8_t gArea[kMaxArea];

// Finds the JWT in a text record's payload: it sits after the status byte and language code
// (normally 3 bytes), so locate it by its fixed header prefix instead of trusting that layout.
void takeToken(const uint8_t* p, size_t len, TagRead& out) {
	for (size_t i = 0; i + 3 <= len && i <= 8; i++) {
		if (p[i] == 'e' && p[i + 1] == 'y' && p[i + 2] == 'J') {
			size_t n = len - i;
			if (n <= kMaxTokenLen) {
				memcpy(out.token, p + i, n);
				out.token[n] = 0;
				out.tokenLen = n;
				out.hasToken = true;
			}
			return;
		}
	}
}

// Returns the payload of the first record of an NDEF message, or nullptr.
const uint8_t* firstRecordPayload(const uint8_t* m, size_t len, size_t& payloadLen) {
	if (len < 3) return nullptr;
	uint8_t hdr = m[0];
	size_t typeLen = m[1];
	size_t i = 2;
	if (hdr & 0x10) {  // short record: 1-byte payload length
		payloadLen = m[i++];
	} else {
		if (len < 6) return nullptr;
		payloadLen = (size_t)m[2] << 24 | (size_t)m[3] << 16 | (size_t)m[4] << 8 | m[5];
		i = 6;
	}
	size_t idLen = 0;
	if (hdr & 0x08) {
		if (i >= len) return nullptr;
		idLen = m[i++];
	}
	i += typeLen + idLen;
	if (i > len || payloadLen > len - i) return nullptr;
	return m + i;
}
}  // namespace

// READ returns 16 bytes (4 pages) per exchange; the NDEF library reads 4 and handles neither
// messages over 254 bytes (3-byte TLV length) nor tags past page 63, which tokens need.
bool Reader::readPages(uint8_t page, uint8_t* out) {
	uint8_t cmd[2] = {0x30, page};
	uint8_t resp[17];
	uint8_t len = sizeof(resp);
	if (!pn532_.inDataExchange(cmd, sizeof(cmd), resp, &len) || len < 16) return false;
	memcpy(out, resp, 16);
	return true;
}

void Reader::readType2(TagRead& out) {
	size_t have = 0;
	auto need = [&](size_t n) {
		if (n > kMaxArea) return false;
		while (have < n) {
			if (4 + have / 4 > kLastPage || !readPages((uint8_t)(4 + have / 4), gArea + have)) return false;
			have += 16;
		}
		return true;
	};
	// Walk the TLVs: skip NULL, lock and memory control blocks until the NDEF message (0x03).
	size_t i = 0;
	for (;;) {
		if (!need(i + 1)) return;
		uint8_t t = gArea[i];
		if (t == 0x00) { i++; continue; }
		if (t == 0xFE) return;
		if (!need(i + 4)) return;
		size_t len = gArea[i + 1];
		size_t hdr = 2;
		if (len == 0xFF) {
			len = (size_t)gArea[i + 2] << 8 | gArea[i + 3];
			hdr = 4;
		}
		if (t != 0x03) { i += hdr + len; continue; }
		if (!need(i + hdr + len)) return;
		size_t payloadLen;
		const uint8_t* p = firstRecordPayload(gArea + i + hdr, len, payloadLen);
		if (p) takeToken(p, payloadLen, out);
		return;
	}
}

bool Reader::probe() {
	// The PN532 library retries a silent chip for seconds; one address ping tells us in ~1 ms.
	Wire.begin();
	// 100 kHz: at 400 kHz the PN532's clock stretching corrupts ESP32 reads mid-tag (MIFARE Classic
	// then halts and every later block fails authentication).
	Wire.setClock(100000);
	Wire.beginTransmission(kPn532Addr);
	if (Wire.endTransmission() != 0) return false;
	nfc_.begin();
	uint32_t t0 = millis();
	uint32_t version = pn532_.getFirmwareVersion();
	LOG("PN532 command round trip: %lu ms.", (unsigned long)(millis() - t0));
	if (!version) return false;
	LOG("PN532 ready (firmware %u.%u).", (unsigned)((version >> 16) & 0xFF), (unsigned)((version >> 8) & 0xFF));
	return true;
}

void Reader::begin() {
	lastCheckMs_ = millis();
	ready_ = probe();
	if (!ready_) LOG("PN532 not found; the door runs without NFC and keeps retrying.");
}

bool Reader::poll(TagRead& out) {
	uint32_t ms = millis();
	if (!ready_) {
		if (ms - lastCheckMs_ < kRetryEveryMs) return false;
		lastCheckMs_ = ms;
		ready_ = probe();
		if (!ready_) LOG("PN532 still not answering.");
		return false;
	}
	if (ms - lastCheckMs_ >= kHealthEveryMs) {
		lastCheckMs_ = ms;
		Wire.beginTransmission(kPn532Addr);
		if (Wire.endTransmission() != 0 || !pn532_.getFirmwareVersion()) {
			ready_ = false;
			LOG("PN532 stopped answering; retrying in the background.");
			return false;
		}
	}
	uint32_t pollStartMs = millis();
	if (!nfc_.tagPresent(kPollMs)) {
		if (present_ && ms - lastSeenMs_ > kReleaseMs) present_ = false;
		return false;
	}
	lastSeenMs_ = ms;
	if (present_) return false;  // still the tag we already handled
	present_ = true;
	uint32_t startMs = millis();

	memset(&out, 0, sizeof(out));
	uint8_t uidLen = nfc_.uidSize();
	if (uidLen == 0 || uidLen > sizeof(out.uid)) return false;
	out.uidLen = uidLen;
	memcpy(out.uid, nfc_.uidBytes(), uidLen);
	if (out.uidLen == 7) {
		readType2(out);  // NTAG / Ultralight, what phones write
		LOG("Tag read in %lu ms (detect %lu ms, uid %u bytes, token %s).", (unsigned long)(millis() - startMs),
	    (unsigned long)(startMs - pollStartMs), (unsigned)out.uidLen, out.hasToken ? "yes" : "no");
		return true;
	}

	// 4-byte UID: MIFARE Classic, read through the NDEF library.
	NfcTag tag = nfc_.read();
	if (tag.hasNdefMessage()) {
		NdefMessage message = tag.getNdefMessage();
		if (message.getRecordCount() > 0) {
			NdefRecord record = message.getRecord(0);
			int len = record.getPayloadLength();
			if (len > 0 && (size_t)len <= kMaxPayload) {
				record.getPayload(gPayload);
				takeToken(gPayload, (size_t)len, out);
			}
		}
	}
	LOG("Tag read in %lu ms (detect %lu ms, uid %u bytes, token %s).", (unsigned long)(millis() - startMs),
	    (unsigned long)(startMs - pollStartMs), (unsigned)out.uidLen, out.hasToken ? "yes" : "no");
	return true;
}
