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
constexpr uint8_t kDefaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // factory key B
constexpr size_t kClassicDataBlocks = 45;  // 3 data blocks in each of sectors 1 to 15

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

// Type 2 capability container (page 3): E1, version, size / 8, access.
constexpr uint8_t kCcPage = 3;
constexpr uint8_t kFirstDataPage = 4;

// Writes one page (WRITE, 0xA2).
bool writePage(Pn532& pn, uint8_t page, const uint8_t* data) {
	uint8_t cmd[6] = {0xA2, page};
	memcpy(cmd + 2, data, 4);
	uint8_t r[1];
	return pn.exchange(cmd, sizeof(cmd), r, sizeof(r)) >= 0;
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

	if (writeActive(writeState_.load(std::memory_order_acquire)) && (int32_t)(ms - jobDeadlineMs_) >= 0)
		finishWrite(WriteState::Timeout);

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
	if (writeActive(writeState_.load(std::memory_order_acquire))) {
		if (present_) return;  // already handled; it has to leave the reader first
		present_ = true;
		write(target);
		return;
	}
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

bool Reader::arm(const char* token, size_t len, const uint8_t* uid, uint8_t uidLen, uint32_t windowMs) {
	if (writeState_.load(std::memory_order_acquire) == WriteState::Writing || uidLen == 0 || uidLen > sizeof(jobUid_))
		return false;
	// The reader task only touches the job once it sees Armed, so quiet it first.
	writeState_.store(WriteState::Idle, std::memory_order_release);
	jobLen_ = ndefEncodeToken(token, len, job_, sizeof(job_));
	if (!jobLen_) return false;
	memset(job_ + jobLen_, 0, sizeof(job_) - jobLen_);
	memcpy(jobUid_, uid, uidLen);
	jobUidLen_ = uidLen;
	scan_ = false;
	jobDeadlineMs_ = millis() + windowMs;
	present_ = false;  // a tag already resting on the reader is taken as the one to write
	writeState_.store(WriteState::Armed, std::memory_order_release);
	return true;
}

bool Reader::armScan(uint32_t windowMs) {
	if (writeState_.load(std::memory_order_acquire) == WriteState::Writing) return false;
	writeState_.store(WriteState::Idle, std::memory_order_release);
	scan_ = true;
	jobUidLen_ = 0;
	jobDeadlineMs_ = millis() + windowMs;
	present_ = false;
	writeState_.store(WriteState::Armed, std::memory_order_release);
	return true;
}

void Reader::cancelWrite() {
	if (writeActive(writeState_.load(std::memory_order_acquire)) &&
	    writeState_.load(std::memory_order_acquire) != WriteState::Writing)
		finishWrite(WriteState::Cancelled);
}

void Reader::write(const Pn532::Target& target) {
	if (scan_) {
		jobUidLen_ = target.uidLen;
		memcpy(jobUid_, target.uid, target.uidLen);
		finishWrite(WriteState::Done);
		return;
	}
	if (target.uidLen != jobUidLen_ || memcmp(target.uid, jobUid_, jobUidLen_) != 0) {
		finishWrite(WriteState::WrongTag);
		LOG("Writer: a different tag was presented.");
		return;
	}
	finishWrite(WriteState::Writing);
	uint32_t startMs = millis();
	WriteState result = writeTo(target);
	LOG("Writer: %s after %lu ms.", writeStateName(result), (unsigned long)(millis() - startMs));
	finishWrite(result);
}

WriteState Reader::writeTo(const Pn532::Target& target) {
	if (target.sak == 0x00) return writeType2(target);
	if (target.sak & 0x08) return writeClassic(target);
	return WriteState::Unsupported;
}

// Writes the job to a Type 2 tag, last page first so a tag pulled away half way never reads as a
// valid (but wrong) key, then reads it back.
WriteState Reader::writeType2(const Pn532::Target& target) {
	const uint8_t readCc[2] = {0x30, kCcPage};
	uint8_t cc[17];
	if (pn_.exchange(readCc, sizeof(readCc), cc, sizeof(cc)) != 16) return WriteState::Failed;
	if (cc[0] != 0xE1) return WriteState::Unsupported;  // not formatted for NDEF
	size_t pages = (jobLen_ + 3) / 4;
	if (pages * 4 > (size_t)cc[2] * 8) return WriteState::TooSmall;

	for (size_t p = pages; p-- > 0;)
		if (!writePage(pn_, (uint8_t)(kFirstDataPage + p), job_ + p * 4)) return WriteState::Failed;

	Fetch fetch = {&pn_, &target, area_, 0, kFirstDataPage};
	if (!fillType2(&fetch, jobLen_) || memcmp(area_, job_, jobLen_) != 0) return WriteState::Failed;
	return WriteState::Done;
}

// MIFARE Classic cards that are already formatted for NDEF (readable with the NDEF key A, which
// is also what the door reads them with). Their data blocks are normally written with key B, the
// factory default; key A is tried too in case the card allows it. A failed authentication leaves
// the card halted, so the card is found again before each new key. Blocks go last first.
WriteState Reader::writeClassic(const Pn532::Target& target) {
	size_t blocks = (jobLen_ + 15) / 16;
	if (blocks > kClassicDataBlocks) return WriteState::TooSmall;

	const uint8_t* uidTail = target.uid + target.uidLen - 4;
	auto auth = [&](uint8_t type, const uint8_t* key, uint8_t block) {
		uint8_t cmd[12] = {type, block};
		memcpy(cmd + 2, key, 6);
		memcpy(cmd + 8, uidTail, 4);
		uint8_t r[1];
		return pn_.exchange(cmd, sizeof(cmd), r, sizeof(r)) >= 0;
	};
	auto refind = [&]() {
		Pn532::Target again;
		return pn_.detect(again) == Pn532::Detect::Found;
	};

	// Only touch cards we can read back.
	if (!auth(0x60, kNdefKey, 4)) return WriteState::Unsupported;

	// Data blocks 4, 5, 6, 8, ... (every fourth block is a sector trailer).
	uint8_t list[kClassicDataBlocks];
	uint8_t b = 4;
	for (size_t i = 0; i < blocks; i++, b++) {
		if (b % 4 == 3) b++;
		list[i] = b;
	}
	for (size_t i = blocks; i-- > 0;) {
		struct Attempt { uint8_t type; const uint8_t* key; };
		const Attempt attempts[] = {{0x61, kDefaultKey}, {0x60, kNdefKey}};
		bool written = false;
		for (const Attempt& a : attempts) {
			if (!refind()) return WriteState::Failed;
			uint8_t cmd[18] = {0xA0, list[i]};
			memcpy(cmd + 2, job_ + i * 16, 16);
			uint8_t r[1];
			if (auth(a.type, a.key, list[i]) && pn_.exchange(cmd, sizeof(cmd), r, sizeof(r)) >= 0) {
				written = true;
				break;
			}
		}
		if (!written) return WriteState::Failed;
	}

	if (!refind()) return WriteState::Failed;
	Fetch fetch = {&pn_, &target, area_, 0, 4};
	if (!fillClassic(&fetch, jobLen_) || memcmp(area_, job_, jobLen_) != 0) return WriteState::Failed;
	return WriteState::Done;
}

void Reader::printStatus() const {
	LOG("nfc       : %s, task stack headroom %u of %u bytes", ready_ ? "ready" : "not answering",
	    (unsigned)uxTaskGetStackHighWaterMark(task_), (unsigned)kTaskStack);
}
