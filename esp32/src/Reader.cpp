#include "Reader.h"
#include <string.h>
#include "Log.h"
#include "Ndef.h"

namespace {
// The token's TLV needs 4 bytes of TLV header, 7 of record header and 3 of text record prefix.
static_assert(kMaxTokenLen + 14 <= Reader::kAreaBytes, "tag area too small for a maximum-size token");

// Pause between polls, so a tag resting on the reader does not keep the chip transmitting flat out.
constexpr uint32_t kIdlePollMs = 5;
// A tag must be gone this long before it can be read again.
constexpr uint32_t kReleaseMs = 600;
constexpr uint32_t kHealthEveryMs = 300000;
constexpr uint32_t kRetryEveryMs = 10000;
// Consecutive bus or protocol failures before the chip is reset and reconfigured.
constexpr uint8_t kMaxErrors = 5;

constexpr uint8_t kTaskCore = 1;
constexpr UBaseType_t kTaskPriority = 2;  // above loop(), which runs at 1 on the same core
constexpr uint32_t kTaskStack = 4096;

// MIFARE Classic's public NDEF key (key A of every data sector).
constexpr uint8_t kNdefKey[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};
constexpr uint8_t kLastPage = 230;   // Type 2 user memory ends at page 225 on an NTAG216
constexpr uint8_t kLastBlock = 63;   // MIFARE Classic 1K

// State of one tag read, shared with the fill callbacks. Data starts at page 4 / block 4.
struct Fetch {
	Pn532* pn;
	const Pn532::Target* target;
	uint8_t* area;
	size_t have;   // bytes of the data area read so far (always a multiple of 16)
	uint8_t next;  // next page (Type 2) or block (Classic) to read
};

// Reads 16 bytes at `address` (READ, 0x30) into the data area.
bool readChunk(Fetch& f, uint8_t address) {
	const uint8_t cmd[2] = {0x30, address};
	uint8_t r[17];  // status byte + 16 data bytes
	if (f.pn->exchange(cmd, sizeof(cmd), r, sizeof(r)) != 16) return false;
	memcpy(f.area + f.have, r, 16);
	f.have += 16;
	return true;
}

// Type 2 tags: one READ returns four pages.
bool fillType2(void* ctx, size_t n) {
	Fetch& f = *static_cast<Fetch*>(ctx);
	if (n > Reader::kAreaBytes) return false;
	while (f.have < n) {
		if (f.next > kLastPage || !readChunk(f, f.next)) return false;
		f.next += 4;
	}
	return true;
}

// MIFARE Classic: one block per read, one authentication per sector, trailers skipped.
bool fillClassic(void* ctx, size_t n) {
	Fetch& f = *static_cast<Fetch*>(ctx);
	if (n > Reader::kAreaBytes) return false;
	while (f.have < n) {
		if (f.next % 4 == 3) f.next++;  // the sector trailer holds keys, not data
		if (f.next > kLastBlock) return false;
		if (f.next % 4 == 0) {
			// AUTH_A, block, key, then the 4 least significant UID bytes.
			uint8_t cmd[12] = {0x60, f.next};
			memcpy(cmd + 2, kNdefKey, sizeof(kNdefKey));
			memcpy(cmd + 8, f.target->uid + f.target->uidLen - 4, 4);
			uint8_t r[2];
			if (f.pn->exchange(cmd, sizeof(cmd), r, sizeof(r)) < 0) return false;
		}
		if (!readChunk(f, f.next)) return false;
		f.next++;
	}
	return true;
}
}  // namespace

bool Reader::probe() {
	if (!pn_.begin(sda_, scl_)) return false;
	uint32_t v = pn_.version();
	LOG("PN532 ready (firmware %u.%u).", (unsigned)((v >> 16) & 0xFF), (unsigned)((v >> 8) & 0xFF));
	return true;
}

void Reader::begin(uint8_t sda, uint8_t scl) {
	sda_ = sda;
	scl_ = scl;
	lastCheckMs_ = millis();
	ready_ = probe();
	if (!ready_) LOG("PN532 not found; the door runs without NFC and keeps retrying.");
	xTaskCreatePinnedToCore(&Reader::run, "nfc", kTaskStack, this, kTaskPriority, &task_, kTaskCore);
}

void Reader::run(void* self) { static_cast<Reader*>(self)->loop(); }

void Reader::loop() {
	for (;;) {
		if (ready_) {
			poll();
			vTaskDelay(pdMS_TO_TICKS(kIdlePollMs));
		} else {
			vTaskDelay(pdMS_TO_TICKS(kRetryEveryMs));
			ready_ = probe();
			if (ready_) errors_ = 0;
			else LOG("PN532 still not answering.");
		}
	}
}

void Reader::poll() {
	uint32_t ms = millis();
	if (ms - lastCheckMs_ >= kHealthEveryMs) {
		lastCheckMs_ = ms;
		if (!pn_.ping()) {
			ready_ = false;
			LOG("PN532 stopped answering; retrying in the background.");
			return;
		}
	}

	Pn532::Target target;
	switch (pn_.detect(target)) {
		case Pn532::Detect::Error:
			if (++errors_ >= kMaxErrors) {  // reset the bus and the chip before giving up on it
				errors_ = 0;
				LOG("PN532 failed %u polls in a row; resetting it.", (unsigned)kMaxErrors);
				ready_ = probe();
				if (!ready_) LOG("PN532 stopped answering; retrying in the background.");
			}
			return;
		case Pn532::Detect::None:
			errors_ = 0;
			if (present_ && ms - lastSeenMs_ > kReleaseMs) present_ = false;
			return;
		case Pn532::Detect::Found:
			break;
	}
	errors_ = 0;
	lastSeenMs_ = millis();
	if (present_ || full_.load(std::memory_order_acquire)) return;  // already handled, or loop() is busy
	present_ = true;
	read(target);
}

void Reader::read(const Pn532::Target& target) {
	uint32_t startMs = millis();
	memset(&slot_, 0, sizeof(slot_));
	slot_.uidLen = target.uidLen;
	memcpy(slot_.uid, target.uid, target.uidLen);

	NdefFill fill = target.sak == 0x00 ? fillType2 : (target.sak & 0x08) ? fillClassic : nullptr;
	Fetch fetch = {&pn_, &target, area_, 0, 4};
	const uint8_t* token;
	size_t len;
	if (fill && ndefFindToken(area_, fill, &fetch, token, len) && len <= kMaxTokenLen) {
		slot_.token = reinterpret_cast<const char*>(token);
		slot_.tokenLen = len;
	}
	LOG("Tag read in %lu ms (uid %u bytes, %s).", (unsigned long)(millis() - startMs), (unsigned)slot_.uidLen,
	    slot_.token ? "token found" : "no token");
	full_.store(true, std::memory_order_release);
}

void Reader::printStatus() const {
	LOG("nfc       : %s, task stack headroom %u of %u bytes", ready_ ? "ready" : "not answering",
	    (unsigned)uxTaskGetStackHighWaterMark(task_), (unsigned)kTaskStack);
}
