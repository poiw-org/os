#include "Persist.h"
#include <LittleFS.h>
#include <atomic>
#include <mutex>
#include <string.h>
#include "Crc.h"
#include "Log.h"

namespace {
const char* kBlacklist = "/blacklist.bin";
const char* kClock = "/clock.bin";
const char* kSigningKey = "/signing.bin";
const char* kJournal = "/journal.bin";
const char* kTmp = "/tmp.bin";
constexpr uint32_t kClockMagic = 0x314B4C43;  // "CLK1"
constexpr size_t kShed = 32;                  // records dropped at once when the journal is full

std::mutex gLock;
// Counts are read without the lock (status output, "is anything queued?"), so they are atomic.
std::atomic<size_t> gJournalCount{0};
std::atomic<uint32_t> gDropped{0};
uint32_t gFront = 0;  // records removed from the front of the journal since boot; guarded by gLock
// Staging area for the blacklist file, shared by loading and saving: no heap use at runtime.
uint8_t gBlacklistImage[BlacklistSet::kMaxSerialized];

// Writes `data` to a temp file and renames it over `path`.
bool replaceFile(const char* path, const uint8_t* data, size_t len) {
	File f = LittleFS.open(kTmp, "w");
	if (!f) return false;
	size_t written = len ? f.write(data, len) : 0;
	f.close();
	if (written != len) { LittleFS.remove(kTmp); return false; }
	LittleFS.remove(path);
	return LittleFS.rename(kTmp, path);
}

// Replaces `path` with `len` bytes of itself starting at `offset`, copied through a small buffer.
bool keepRange(const char* path, size_t offset, size_t len) {
	File in = LittleFS.open(path, "r");
	if (!in) return false;
	File out = LittleFS.open(kTmp, "w");
	if (!out) { in.close(); return false; }
	uint8_t chunk[256];
	bool ok = in.seek(offset);
	for (size_t done = 0; ok && done < len;) {
		size_t n = len - done < sizeof(chunk) ? len - done : sizeof(chunk);
		ok = in.read(chunk, n) == n && out.write(chunk, n) == n;
		done += n;
	}
	in.close();
	out.close();
	if (!ok) { LittleFS.remove(kTmp); return false; }
	LittleFS.remove(path);
	return LittleFS.rename(kTmp, path);
}

// Removes the oldest `count` records. The caller holds gLock.
bool dropFront(size_t count) {
	size_t have = gJournalCount.load();
	if (!count || !have) return true;
	if (count >= have) {
		if (LittleFS.exists(kJournal) && !LittleFS.remove(kJournal)) return false;
		gFront += (uint32_t)have;
		gJournalCount = 0;
		return true;
	}
	if (!keepRange(kJournal, count * sizeof(JournalRecord), (have - count) * sizeof(JournalRecord))) return false;
	gFront += (uint32_t)count;
	gJournalCount = have - count;
	return true;
}
}  // namespace

namespace Persist {

bool begin() {
	std::lock_guard<std::mutex> hold(gLock);
	gJournalCount = 0;
	gDropped = 0;
	gFront = 0;
	if (!LittleFS.begin(false)) {
		// First boot or a damaged filesystem.
		LOG("Formatting flash storage...");
		bool ok = LittleFS.format() && LittleFS.begin(false);
		if (!ok) { LOG("Flash storage is unusable; running without persistence."); return false; }
	}
	LittleFS.remove(kTmp);  // leftover of an interrupted replace

	// Repair the journal: a power cut during an append leaves a partial record at the end.
	File f = LittleFS.open(kJournal, "r");
	if (f) {
		size_t size = f.size();
		f.close();
		gJournalCount = size / sizeof(JournalRecord);
		if (size % sizeof(JournalRecord)) {
			LOG("Journal had a torn tail; repairing.");
			keepRange(kJournal, 0, gJournalCount.load() * sizeof(JournalRecord));
		}
	}
	return true;
}

bool loadBlacklist(BlacklistSet& out) {
	std::lock_guard<std::mutex> hold(gLock);
	out.clear();
	File f = LittleFS.open(kBlacklist, "r");
	if (!f) return false;
	size_t size = f.size();
	if (size < 12 || size > sizeof(gBlacklistImage)) { f.close(); return false; }
	bool ok = f.read(gBlacklistImage, size) == size && out.deserialize(gBlacklistImage, size);
	f.close();
	if (!ok) out.clear();
	return ok;
}

bool saveBlacklist(const BlacklistSet& set) {
	std::lock_guard<std::mutex> hold(gLock);
	size_t n = set.serialize(gBlacklistImage, sizeof(gBlacklistImage));
	return n && replaceFile(kBlacklist, gBlacklistImage, n);
}

bool loadSigningKey(uint8_t n[kRsaBytes]) {
	std::lock_guard<std::mutex> hold(gLock);
	File f = LittleFS.open(kSigningKey, "r");
	if (!f || f.size() != kRsaBytes + 4) return false;
	uint8_t image[kRsaBytes + 4];
	bool ok = f.read(image, sizeof(image)) == sizeof(image);
	f.close();
	uint32_t sum;
	memcpy(&sum, image + kRsaBytes, 4);
	if (!ok || sum != crc32(image, kRsaBytes)) return false;
	memcpy(n, image, kRsaBytes);
	return true;
}

bool saveSigningKey(const uint8_t n[kRsaBytes]) {
	uint8_t image[kRsaBytes + 4];
	memcpy(image, n, kRsaBytes);
	uint32_t sum = crc32(n, kRsaBytes);
	memcpy(image + kRsaBytes, &sum, 4);
	std::lock_guard<std::mutex> hold(gLock);
	return replaceFile(kSigningKey, image, sizeof(image));
}

uint32_t loadClock() {
	std::lock_guard<std::mutex> hold(gLock);
	File f = LittleFS.open(kClock, "r");
	if (!f || f.size() != 12) return 0;
	uint32_t v[3];
	bool ok = f.read((uint8_t*)v, 12) == 12;
	f.close();
	return (ok && v[0] == kClockMagic && v[2] == crc32((const uint8_t*)v, 8)) ? v[1] : 0;
}

bool saveClock(uint32_t epoch) {
	uint32_t v[3] = {kClockMagic, epoch, 0};
	v[2] = crc32((const uint8_t*)v, 8);
	std::lock_guard<std::mutex> hold(gLock);
	return replaceFile(kClock, (const uint8_t*)v, sizeof(v));
}

bool journalDrop(size_t count, uint32_t seq) {
	std::lock_guard<std::mutex> hold(gLock);
	uint32_t shed = gFront - seq;  // already gone from the front since the caller read it
	return dropFront(count > shed ? count - shed : 0);
}

bool journalAppend(JournalRecord record) {
	journalSeal(record);
	std::lock_guard<std::mutex> hold(gLock);
	if (gJournalCount.load() >= kJournalCapacity) {
		LOG("Journal full; dropping the %u oldest events.", (unsigned)kShed);
		if (dropFront(kShed)) gDropped += (uint32_t)kShed;
	}
	File f = LittleFS.open(kJournal, "a");
	if (!f) return false;
	size_t n = f.write((const uint8_t*)&record, sizeof(record));
	f.close();
	if (n != sizeof(record)) return false;
	gJournalCount++;
	return true;
}

size_t journalCount() { return gJournalCount.load(); }
uint32_t journalDropped() { return gDropped.load(); }

size_t journalRead(JournalRecord* out, size_t max, uint32_t* seq) {
	std::lock_guard<std::mutex> hold(gLock);
	*seq = gFront;
	File f = LittleFS.open(kJournal, "r");
	if (!f) return 0;
	size_t n = 0;
	while (n < max && f.read((uint8_t*)&out[n], sizeof(JournalRecord)) == sizeof(JournalRecord)) n++;
	f.close();
	return n;
}

}  // namespace Persist
