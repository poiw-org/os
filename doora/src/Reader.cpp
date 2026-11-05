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
// A read that fails on the radio link (tag at the edge of the field, slid away) is retried this
// many times before the tap is handed out without a token.
constexpr uint8_t kMaxReadRetries = 2;
// While the chip is down, how often the reader task wakes to check a write session's deadline.
constexpr uint32_t kDownTickMs = 250;

constexpr uint8_t kTaskCore = 1;
constexpr UBaseType_t kTaskPriority = 2;  // above loop(), which runs at 1 on the same core
constexpr uint32_t kTaskStack = 4096;

// MIFARE Classic's public NDEF key (key A of every data sector).
constexpr uint8_t kNdefKey[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};
constexpr uint8_t kLastPage = 230;   // Type 2 user memory ends at page 225 on an NTAG216
// Pages per FAST_READ: 240 bytes, which keeps the answer inside one normal PN532 frame.
constexpr size_t kFastPages = 60;
static_assert(kFastPages * 4 <= Pn532::kMaxThroughData, "a FAST_READ answer must fit one frame");
// Sector trailer parts for NDEF formatting (NXP AN1304): MAD key A and access bits + GPB (MAD v1),
// and the access bits of the NDEF data sectors.
constexpr uint8_t kMadKey[6] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
constexpr uint8_t kMadAccess[4] = {0x78, 0x77, 0x88, 0xC1};
constexpr uint8_t kNdefAccess[4] = {0x7F, 0x07, 0x88, 0x40};
constexpr uint8_t kLastBlock = 63;   // MIFARE Classic 1K
constexpr uint8_t kDefaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // factory key B
constexpr size_t kClassicDataBlocks = 45;  // 3 data blocks in each of sectors 1 to 15

// State of one tag read, shared with the fill callbacks. Data starts at page 4 / block 4.
struct Fetch {
	Pn532* pn;
	const Pn532::Target* target;
	uint8_t* area;
	size_t have;   // bytes of the data area read so far (a multiple of 4; of 16 on Classic)
	uint8_t next;  // next page (Type 2) or block (Classic) to read
	bool fast = true;  // Type 2: the tag has not refused FAST_READ yet
	bool ioError = false;  // a read failed on the link, as opposed to data that is not a key
};

// Reads 16 bytes at `address` (READ, 0x30) into the data area, as many as fit.
bool readChunk(Fetch& f, uint8_t address) {
	const uint8_t cmd[2] = {0x30, address};
	uint8_t r[17];  // status byte + 16 data bytes
	if (f.pn->exchange(cmd, sizeof(cmd), r, sizeof(r)) != 16) { f.ioError = true; return false; }
	size_t room = Reader::kAreaBytes - f.have;
	size_t take = room < 16 ? room : 16;
	memcpy(f.area + f.have, r, take);
	f.have += take;
	return true;
}

// FAST_READ (0x3A; NTAG and Ultralight EV1): `pages` pages from f.next in one exchange.
bool fastRead(Fetch& f, size_t pages) {
	const uint8_t cmd[3] = {0x3A, f.next, (uint8_t)(f.next + pages - 1)};
	uint8_t r[kFastPages * 4 + 1];  // status byte + data
	if (f.pn->communicate(cmd, sizeof(cmd), r, sizeof(r)) != (int)(pages * 4)) return false;
	memcpy(f.area + f.have, r, pages * 4);
	f.have += pages * 4;
	f.next += (uint8_t)pages;
	return true;
}

// Type 2 tags. FAST_READ takes up to 60 pages per exchange where READ takes 4, which is what
// makes a tap fast: each exchange costs a few milliseconds of bus and chip time whatever its size.
// FAST_READ reads exactly the pages asked for (READ would wrap past the end of the tag, FAST_READ
// is refused), so it never asks for more than the NDEF data needs, but at least 4 pages so the
// small TLV header reads at the start do not each cost an exchange.
bool fillType2(void* ctx, size_t n) {
	Fetch& f = *static_cast<Fetch*>(ctx);
	if (n > Reader::kAreaBytes) return false;
	while (f.have < n) {
		if (f.fast) {
			size_t pages = (n - f.have + 3) / 4;
			if (pages < 4) pages = 4;
			if (pages > kFastPages) pages = kFastPages;
			if (f.have + pages * 4 > Reader::kAreaBytes) pages = (Reader::kAreaBytes - f.have) / 4;
			if (f.next + pages - 1 > kLastPage) return false;
			if (fastRead(f, pages)) continue;
			// A tag without FAST_READ (the original Ultralight) answers with a NAK, which halts it:
			// select it again and go on with READ.
			f.fast = false;
			Pn532::Target again;
			if (f.pn->detect(again) != Pn532::Detect::Found || again.uidLen != f.target->uidLen ||
			    memcmp(again.uid, f.target->uid, again.uidLen) != 0) {
				f.ioError = true;
				return false;
			}
		}
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
			// A refused key means the card is not NDEF formatted: final, not a link error.
			int got = f.pn->exchange(cmd, sizeof(cmd), r, sizeof(r));
			if (got < 0) {
				if (got != Pn532::kErrTag) f.ioError = true;
				return false;
			}
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
	nextProbeMs_ = lastCheckMs_ + kRetryEveryMs;
	ready_ = probe();
	if (!ready_) LOG("PN532 not found; the door runs without NFC and keeps retrying.");
	xTaskCreatePinnedToCore(&Reader::run, "nfc", kTaskStack, this, kTaskPriority, &task_, kTaskCore);
}

void Reader::run(void* self) { static_cast<Reader*>(self)->loop(); }

void Reader::loop() {
	for (;;) {
		checkDeadline();
		if (ready_) {
			poll();
			vTaskDelay(pdMS_TO_TICKS(kIdlePollMs));
			if (!ready_) nextProbeMs_ = millis() + kRetryEveryMs;
		} else if ((int32_t)(millis() - nextProbeMs_) >= 0) {
			ready_ = probe();
			if (ready_) errors_ = 0;
			else {
				LOG("PN532 still not answering (SDA %d, SCL %d).", pn_.lines() & 1, pn_.lines() >> 1);
				nextProbeMs_ = millis() + kRetryEveryMs;
			}
		} else {
			vTaskDelay(pdMS_TO_TICKS(kDownTickMs));
		}
	}
}

bool Reader::leaveWaiting(WriteState to) {
	WriteState s = writeState_.load(std::memory_order_acquire);
	while (s == WriteState::Armed || s == WriteState::WrongTag)
		if (writeState_.compare_exchange_weak(s, to, std::memory_order_acq_rel, std::memory_order_acquire)) return true;
	return false;
}

void Reader::checkDeadline() {
	WriteState s = writeState_.load(std::memory_order_acquire);
	if ((s == WriteState::Armed || s == WriteState::WrongTag) && (int32_t)(millis() - jobDeadlineMs_) >= 0)
		leaveWaiting(WriteState::Timeout);
}

void Reader::poll() {
	uint32_t ms = millis();
	if (holding_.load(std::memory_order_acquire)) {
		if ((int32_t)(ms - holdUntilMs_.load(std::memory_order_acquire)) < 0) return;
		holding_.store(false, std::memory_order_release);
	}
	if (ms - lastCheckMs_ >= kHealthEveryMs) {
		lastCheckMs_ = ms;
		// One missed answer on a bit-banged bus is not a dead chip: reset it at once (probe() retries
		// itself) and only fall back to the slow background retry if that fails too.
		if (!pn_.ping()) {
			LOG("PN532 missed a health check; resetting it.");
			ready_ = probe();
			if (!ready_) {
				LOG("PN532 stopped answering; retrying in the background.");
				return;
			}
			errors_ = 0;
		}
	}

	if (rearm_.exchange(false, std::memory_order_acq_rel)) present_ = false;

	Pn532::Target target;
	switch (pn_.detect(target)) {
		case Pn532::Detect::Error:
			if (++errors_ >= kMaxErrors) {  // reset the bus and the chip before giving up on it
				errors_ = 0;
				LOG("PN532 failed %u polls in a row (last error %d: -1 bus, -2 timeout, -3 frame); resetting it.", (unsigned)kMaxErrors,
				    pn_.lastError());
				ready_ = probe();
				if (!ready_) LOG("PN532 stopped answering; retrying in the background.");
			}
			return;
		case Pn532::Detect::None:
			errors_ = 0;
			// retries_ is reset even when present_ is already false (a failed read clears it), so
			// the next tag gets its own retries.
			if (ms - lastSeenMs_ > kReleaseMs) {
				present_ = false;
				retries_ = 0;
			}
			return;
		case Pn532::Detect::Found:
			break;
	}
	errors_ = 0;
	lastSeenMs_ = millis();
	if (writeActive(writeState_.load(std::memory_order_acquire))) {
		// loop() still holds a tag read before the session was armed, and its token points into
		// area_, which the write's read-back reuses: wait for release().
		if (full_.load(std::memory_order_acquire)) return;
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
	slot_.foundMs = startMs;
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
	// A session armed while this read ran: nothing opens the door while armed, and the tag still
	// on the reader is the one to write.
	if (writeActive(writeState_.load(std::memory_order_acquire))) {
		present_ = false;
		retries_ = 0;
		return;
	}
	// A radio-link failure is not a verdict on the key: read the tag again on the next poll.
	if (!slot_.token && fetch.ioError && retries_ < kMaxReadRetries) {
		retries_++;
		present_ = false;
		LOG("Tag read failed on the radio link; retrying.");
		return;
	}
	retries_ = 0;
	uint32_t tookMs = millis() - startMs;
	uint8_t uidLen = slot_.uidLen;
	bool found = slot_.token != nullptr;
	full_.store(true, std::memory_order_release);  // hand it over before logging, which can wait on the UART
	LOG("Tag read in %lu ms (uid %u bytes, %s).", (unsigned long)tookMs, (unsigned)uidLen,
	    found ? "token found" : "no token");
}

bool Reader::quiet() {
	WriteState s = writeState_.load(std::memory_order_acquire);
	do {
		if (s == WriteState::Writing) return false;
	} while (!writeState_.compare_exchange_weak(s, WriteState::Idle, std::memory_order_acq_rel, std::memory_order_acquire));
	return true;
}

bool Reader::arm(const char* token, size_t len, const uint8_t* uid, uint8_t uidLen, uint32_t windowMs) {
	if (uidLen == 0 || uidLen > sizeof(jobUid_)) return false;
	// The reader task only touches the job once it sees Armed, so quiet it first. The state is
	// swapped in one step: a write the reader starts meanwhile makes this fail instead of being
	// overwritten mid-way.
	if (!quiet()) return false;
	jobLen_ = ndefEncodeToken(token, len, job_, sizeof(job_));
	if (!jobLen_) return false;
	memset(job_ + jobLen_, 0, sizeof(job_) - jobLen_);
	memcpy(jobUid_, uid, uidLen);
	jobUidLen_ = uidLen;
	scan_ = false;
	jobDeadlineMs_ = millis() + windowMs;
	rearm_.store(true, std::memory_order_release);  // a tag already resting on the reader is taken as the one to write
	writeState_.store(WriteState::Armed, std::memory_order_release);
	return true;
}

bool Reader::armScan(uint32_t windowMs) {
	if (!quiet()) return false;
	scan_ = true;
	jobUidLen_ = 0;
	jobDeadlineMs_ = millis() + windowMs;
	rearm_.store(true, std::memory_order_release);
	writeState_.store(WriteState::Armed, std::memory_order_release);
	return true;
}

void Reader::cancelWrite() { leaveWaiting(WriteState::Cancelled); }

void Reader::write(const Pn532::Target& target) {
	// Claims the session first (Writing), so loop() can neither rearm nor cancel it underneath.
	if (scan_) {
		if (!leaveWaiting(WriteState::Writing)) return;
		jobUidLen_ = target.uidLen;
		memcpy(jobUid_, target.uid, target.uidLen);
		finishWrite(WriteState::Done);
		return;
	}
	if (target.uidLen != jobUidLen_ || memcmp(target.uid, jobUid_, jobUidLen_) != 0) {
		if (leaveWaiting(WriteState::WrongTag)) LOG("Writer: a different tag was presented.");
		return;
	}
	if (!leaveWaiting(WriteState::Writing)) return;
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

// MIFARE Classic cards. A card that is not NDEF formatted yet (factory keys) is formatted first
// (NXP layout: MAD in sector 0, the NDEF key A on sectors 1 to 15). Data blocks are then written
// with key B, the factory default; key A is tried too in case the card allows it. A failed
// authentication leaves the card halted, so the card is found again before each new key. Blocks go
// last first.
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
	// Finds the card again, authenticates the block's sector and writes the block.
	auto put = [&](uint8_t type, const uint8_t* key, uint8_t block, const uint8_t* data) {
		if (!refind() || !auth(type, key, block)) return false;
		uint8_t cmd[18] = {0xA0, block};
		memcpy(cmd + 2, data, 16);
		uint8_t r[1];
		return pn_.exchange(cmd, sizeof(cmd), r, sizeof(r)) >= 0;
	};

	// Only touch cards we can read back, or blank ones we can format.
	if (!auth(0x60, kNdefKey, 4)) {
		if (!refind() || !auth(0x60, kDefaultKey, 4)) return WriteState::Unsupported;
		// Sectors 15 to 1 first, the MAD and its trailer last: a card pulled away half way is not
		// advertised as NDEF.
		uint8_t trailer[16];
		memcpy(trailer, kNdefKey, 6);
		memcpy(trailer + 6, kNdefAccess, 4);
		memcpy(trailer + 10, kDefaultKey, 6);
		for (uint8_t sector = 15; sector >= 1; sector--)
			if (!put(0x60, kDefaultKey, sector * 4 + 3, trailer)) return WriteState::Failed;
		uint8_t mad[16] = {0x14, 0x01};  // CRC, info byte, then sectors 1 to 7
		for (int i = 2; i < 16; i += 2) { mad[i] = 0x03; mad[i + 1] = 0xE1; }
		if (!put(0x60, kDefaultKey, 1, mad)) return WriteState::Failed;
		for (int i = 0; i < 16; i += 2) { mad[i] = 0x03; mad[i + 1] = 0xE1; }  // sectors 8 to 15
		if (!put(0x60, kDefaultKey, 2, mad)) return WriteState::Failed;
		memcpy(trailer, kMadKey, 6);
		memcpy(trailer + 6, kMadAccess, 4);
		if (!put(0x60, kDefaultKey, 3, trailer)) return WriteState::Failed;
	}

	// Data blocks 4, 5, 6, 8, ... (every fourth block is a sector trailer).
	uint8_t list[kClassicDataBlocks];
	uint8_t b = 4;
	for (size_t i = 0; i < blocks; i++, b++) {
		if (b % 4 == 3) b++;
		list[i] = b;
	}
	for (size_t i = blocks; i-- > 0;) {
		if (!put(0x61, kDefaultKey, list[i], job_ + i * 16) &&
		    !put(0x60, kNdefKey, list[i], job_ + i * 16))
			return WriteState::Failed;
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
