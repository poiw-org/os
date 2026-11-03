#include "Persist.h"
#include <LittleFS.h>
#include <string.h>
#include "Crc.h"
#include "Log.h"

namespace {
const char* kBlacklist = "/blacklist.bin";
const char* kClock = "/clock.bin";
const char* kJournal = "/journal.bin";
const char* kTmp = "/tmp.bin";
constexpr uint32_t kClockMagic = 0x314B4C43;  // "CLK1"

size_t gJournalCount = 0;
uint32_t gDropped = 0;

// Writes `data` to a temp file and renames it over `path`. rename() is atomic on LittleFS.
bool replaceFile(const char* path, const uint8_t* data, size_t len) {
	File f = LittleFS.open(kTmp, "w");
	if (!f) return false;
	size_t written = len ? f.write(data, len) : 0;
	f.close();
	if (written != len) { LittleFS.remove(kTmp); return false; }
	LittleFS.remove(path);
	return LittleFS.rename(kTmp, path);
}
}  // namespace

namespace Persist {

bool begin() {
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
		gJournalCount = size / sizeof(JournalRecord);
		if (size % sizeof(JournalRecord)) {
			LOG("Journal had a torn tail; repairing.");
			size_t keep = gJournalCount * sizeof(JournalRecord);
			uint8_t* buf = keep ? (uint8_t*)malloc(keep) : nullptr;
			bool ok = !keep || (buf && f.read(buf, keep) == keep);
			f.close();
			if (ok) replaceFile(kJournal, buf, keep);
			free(buf);
		} else f.close();
	}
	return true;
}

bool loadBlacklist(BlacklistSet& out) {
	out.clear();
	File f = LittleFS.open(kBlacklist, "r");
	if (!f) return false;
	size_t size = f.size();
	if (size < 12 || size > BlacklistSet::kMaxSerialized) { f.close(); return false; }
	uint8_t* buf = (uint8_t*)malloc(size);
	if (!buf) { f.close(); return false; }
	bool ok = f.read(buf, size) == size && out.deserialize(buf, size);
	f.close();
	free(buf);
	if (!ok) out.clear();
	return ok;
}

bool saveBlacklist(const BlacklistSet& set) {
	uint8_t* buf = (uint8_t*)malloc(BlacklistSet::kMaxSerialized);
	if (!buf) return false;
	size_t n = set.serialize(buf, BlacklistSet::kMaxSerialized);
	bool ok = n && replaceFile(kBlacklist, buf, n);
	free(buf);
	return ok;
}

uint32_t loadClock() {
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
	return replaceFile(kClock, (const uint8_t*)v, sizeof(v));
}

bool journalDrop(size_t count) {
	if (!count) return true;
	if (count >= gJournalCount) {
		LittleFS.remove(kJournal);
		gJournalCount = 0;
		return true;
	}
	File f = LittleFS.open(kJournal, "r");
	if (!f) return false;
	size_t keep = (gJournalCount - count) * sizeof(JournalRecord);
	uint8_t* buf = (uint8_t*)malloc(keep);
	bool ok = buf && f.seek(count * sizeof(JournalRecord)) && f.read(buf, keep) == keep;
	f.close();
	ok = ok && replaceFile(kJournal, buf, keep);
	free(buf);
	if (ok) gJournalCount -= count;
	return ok;
}

bool journalAppend(JournalRecord record) {
	if (gJournalCount >= kJournalCapacity) {
		constexpr size_t kShed = 32;
		LOG("Journal full; dropping the %u oldest events.", (unsigned)kShed);
		if (journalDrop(kShed)) gDropped += kShed;
	}
	journalSeal(record);
	File f = LittleFS.open(kJournal, "a");
	if (!f) return false;
	size_t n = f.write((const uint8_t*)&record, sizeof(record));
	f.close();
	if (n != sizeof(record)) return false;
	gJournalCount++;
	return true;
}

size_t journalCount() { return gJournalCount; }
uint32_t journalDropped() { return gDropped; }

size_t journalRead(JournalRecord* out, size_t max) {
	File f = LittleFS.open(kJournal, "r");
	if (!f) return 0;
	size_t n = 0;
	while (n < max && f.read((uint8_t*)&out[n], sizeof(JournalRecord)) == sizeof(JournalRecord)) n++;
	f.close();
	return n;
}

}  // namespace Persist
