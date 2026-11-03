#include "Reader.h"
#include <string.h>
#include "Log.h"

namespace {
// How long one presence check waits. Short, so the websocket and relay timers stay responsive.
constexpr uint32_t kPollMs = 60;
// A tag must be gone this long before it can be read again.
constexpr uint32_t kReleaseMs = 600;
constexpr uint32_t kHealthEveryMs = 300000;
constexpr size_t kMaxPayload = kMaxTokenLen + 8;
uint8_t gPayload[kMaxPayload];
}  // namespace

void Reader::begin() {
	nfc_.begin();
	uint32_t version = pn532_.getFirmwareVersion();
	if (version) LOG("PN532 ready (firmware %u.%u).", (unsigned)((version >> 16) & 0xFF), (unsigned)((version >> 8) & 0xFF));
	else LOG("PN532 did not answer; will keep retrying.");
}

bool Reader::healthy() {
	uint32_t ms = millis();
	if (ms - lastCheckMs_ < kHealthEveryMs) return true;
	lastCheckMs_ = ms;
	if (pn532_.getFirmwareVersion()) { failures_ = 0; return true; }
	failures_++;
	LOG("PN532 not answering (%u).", failures_);
	nfc_.begin();
	return failures_ < 3;
}

bool Reader::poll(TagRead& out) {
	uint32_t ms = millis();
	if (!nfc_.tagPresent(kPollMs)) {
		if (present_ && ms - lastSeenMs_ > kReleaseMs) present_ = false;
		return false;
	}
	lastSeenMs_ = ms;
	if (present_) return false;  // still the tag we already handled
	present_ = true;

	NfcTag tag = nfc_.read();
	memset(&out, 0, sizeof(out));
	out.uidLen = (uint8_t)tag.getUidLength();
	if (out.uidLen == 0 || out.uidLen > sizeof(out.uid)) return false;
	tag.getUid(out.uid, out.uidLen);

	if (tag.hasNdefMessage()) {
		NdefMessage message = tag.getNdefMessage();
		if (message.getRecordCount() > 0) {
			NdefRecord record = message.getRecord(0);
			int len = record.getPayloadLength();
			if (len > 0 && (size_t)len <= kMaxPayload) {
				record.getPayload(gPayload);
				// The token sits after the text record's status byte and language code (normally 3
				// bytes). Locate it by its fixed JWT header prefix instead of trusting that layout.
				for (int i = 0; i + 3 <= len && i <= 8; i++) {
					if (gPayload[i] == 'e' && gPayload[i + 1] == 'y' && gPayload[i + 2] == 'J') {
						size_t n = (size_t)len - (size_t)i;
						if (n <= kMaxTokenLen) {
							memcpy(out.token, gPayload + i, n);
							out.token[n] = 0;
							out.tokenLen = n;
							out.hasToken = true;
						}
						break;
					}
				}
			}
		}
	}
	return true;
}
