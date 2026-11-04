// Runs the real Persist.cpp on an in-memory LittleFS: the event journal's counting, draining and
// repair, including the case where the journal sheds its oldest records while the REST task is
// uploading them, and the blacklist and clock files. Build and run with `make test`.
#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include <LittleFS.h>
#include "../src/Persist.h"

uint64_t g_nowUs = 0;
HostSerial Serial;
FakeFs g_fs;
FakeLittleFs LittleFS;
static int failures = 0, checks = 0;
#define CHECK(cond)                                                       \
	do {                                                                  \
		checks++;                                                         \
		if (!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
	} while (0)

static JournalRecord record(uint32_t ts) {
	JournalRecord r;
	memset(&r, 0, sizeof(r));
	r.ts = ts;
	r.allowed = 1;
	return r;
}

static void freshFlash() {
	g_fs = FakeFs();
	CHECK(Persist::begin());
}

static void fill(uint32_t from, uint32_t count) {
	for (uint32_t i = 0; i < count; i++) CHECK(Persist::journalAppend(record(from + i)));
}

static void testJournalBasics() {
	freshFlash();
	CHECK(Persist::journalCount() == 0);
	JournalRecord out[8];
	uint32_t seq;
	CHECK(Persist::journalRead(out, 8, &seq) == 0);

	fill(100, 5);
	CHECK(Persist::journalCount() == 5);
	CHECK(Persist::journalRead(out, 3, &seq) == 3);
	CHECK(out[0].ts == 100 && out[2].ts == 102 && journalValid(out[0]) && journalValid(out[2]));

	// Dropping what was read leaves the rest, in order.
	CHECK(Persist::journalDrop(3, seq));
	CHECK(Persist::journalCount() == 2);
	size_t n = Persist::journalRead(out, 8, &seq);
	CHECK(n == 2 && out[0].ts == 103 && out[1].ts == 104);

	// Records appended after a read are not part of what the drop removes.
	fill(200, 1);
	CHECK(Persist::journalDrop(2, seq));
	n = Persist::journalRead(out, 8, &seq);
	CHECK(n == 1 && out[0].ts == 200);

	// Draining everything removes the file; the journal keeps working afterwards.
	CHECK(Persist::journalDrop(1, seq));
	CHECK(Persist::journalCount() == 0 && !g_fs.files.count("/journal.bin"));
	fill(300, 1);
	CHECK(Persist::journalRead(out, 8, &seq) == 1 && out[0].ts == 300);
	CHECK(Persist::journalDrop(0, seq) && Persist::journalCount() == 1);
}

// The journal is full and sheds its oldest records while an upload is in flight. The upload then
// reports success for what it read: only records that were not already shed may disappear.
static void testShedDuringUpload() {
	freshFlash();
	fill(0, Persist::kJournalCapacity);
	JournalRecord batch[8];
	uint32_t seq;
	CHECK(Persist::journalRead(batch, 8, &seq) == 8 && batch[0].ts == 0);

	fill(1000, 1);  // full: sheds the 32 oldest, which include everything the upload read
	CHECK(Persist::journalDropped() == 32);
	CHECK(Persist::journalCount() == Persist::kJournalCapacity - 32 + 1);

	CHECK(Persist::journalDrop(8, seq));  // all 8 were shed already: nothing more may go
	CHECK(Persist::journalCount() == Persist::kJournalCapacity - 32 + 1);
	JournalRecord head[1];
	uint32_t seq2;
	CHECK(Persist::journalRead(head, 1, &seq2) == 1 && head[0].ts == 32);

	// A bigger upload than the shed: the part beyond what was shed is removed.
	JournalRecord wide[40];
	uint32_t seq3;
	CHECK(Persist::journalRead(wide, 40, &seq3) == 40 && wide[0].ts == 32);
	fill(2000, Persist::kJournalCapacity);  // sheds again, repeatedly, while it is "uploading"
	uint32_t shedSince = 0;
	(void)shedSince;
	size_t before = Persist::journalCount();
	CHECK(Persist::journalDrop(40, seq3));
	CHECK(Persist::journalCount() <= before);
	CHECK(Persist::journalRead(head, 1, &seq2) == 1);
	// Whatever is at the front now was never read by the upload (it is newer than ts 71).
	CHECK(head[0].ts >= 72);
}

// An opening made offline cannot be flushed out of the journal by tapping invalid tags.
static void testDenialsNeverShedOpenings() {
	freshFlash();
	JournalRecord open = record(1);
	CHECK(Persist::journalAppend(open));
	for (uint32_t i = 0; i < 2 * Persist::kJournalCapacity; i++) {
		JournalRecord d = record(100 + i);
		d.allowed = 0;
		CHECK(Persist::journalAppend(d));  // accepted (counted) even when not stored
	}
	JournalRecord head[1];
	uint32_t seq;
	CHECK(Persist::journalRead(head, 1, &seq) == 1 && head[0].ts == 1 && head[0].allowed == 1);
	CHECK(Persist::journalCount() == 1 + 64 && Persist::journalDropped() == 0);
	CHECK(Persist::journalSuppressed() == 2 * Persist::kJournalCapacity - 64);

	// The quota is rebuilt from flash after a reboot, and openings still have room.
	CHECK(Persist::begin());
	JournalRecord d = record(5000);
	d.allowed = 0;
	CHECK(Persist::journalAppend(d) && Persist::journalCount() == 1 + 64);
	fill(6000, Persist::kJournalCapacity - 65);
	CHECK(Persist::journalCount() == Persist::kJournalCapacity && Persist::journalDropped() == 0);

	// Draining denials from the front frees quota again.
	JournalRecord batch[8];
	CHECK(Persist::journalRead(batch, 8, &seq) == 8);
	CHECK(Persist::journalDrop(8, seq));
	CHECK(Persist::journalAppend(d) && Persist::journalCount() == Persist::kJournalCapacity - 7);

	// The shed counter survives a reboot.
	fill(9000, 8);
	uint32_t dropped = Persist::journalDropped();
	CHECK(dropped == 32);
	CHECK(Persist::begin() && Persist::journalDropped() == dropped);
}

// Replacing a file never deletes the old one first: the rename does it atomically.
static void testReplaceKeepsOldUntilRename() {
	freshFlash();
	CHECK(Persist::saveClock(1767225600u));
	g_fs.failWritesAfter = 4;  // the new version cannot be written in full
	CHECK(!Persist::saveClock(1767225700u));
	g_fs.failWritesAfter = -1;
	CHECK(Persist::loadClock() == 1767225600u && !g_fs.files.count("/tmp.bin"));
}

static void testTornTailRepair() {
	freshFlash();
	fill(10, 3);
	g_fs.files["/journal.bin"].resize(g_fs.files["/journal.bin"].size() - 20);  // power cut mid-append
	CHECK(Persist::begin());
	CHECK(Persist::journalCount() == 2);
	JournalRecord out[4];
	uint32_t seq;
	CHECK(Persist::journalRead(out, 4, &seq) == 2 && out[1].ts == 11 && journalValid(out[1]));
	CHECK(g_fs.files["/journal.bin"].size() == 2 * sizeof(JournalRecord));
	CHECK(!g_fs.files.count("/tmp.bin"));
}

static void testFailedWrites() {
	freshFlash();
	fill(1, 4);
	g_fs.failWritesAfter = 100;  // a rewrite runs out of space part-way
	JournalRecord out[4];
	uint32_t seq;
	CHECK(Persist::journalRead(out, 4, &seq) == 4);
	CHECK(!Persist::journalDrop(2, seq));
	g_fs.failWritesAfter = -1;
	CHECK(Persist::journalCount() == 4 && !g_fs.files.count("/tmp.bin"));  // untouched, no debris
	CHECK(Persist::journalRead(out, 4, &seq) == 4 && out[0].ts == 1);
	CHECK(Persist::journalDrop(2, seq) && Persist::journalCount() == 2);
	CHECK(Persist::journalRead(out, 4, &seq) == 2 && out[0].ts == 3);
}

static void testBlacklistFile() {
	freshFlash();
	BlacklistSet a, b;
	CHECK(!Persist::loadBlacklist(b));  // nothing stored yet
	a.upsert("key-1", 5000);
	a.upsert("C-lDM7B2wGY7nURY", 6000);
	CHECK(Persist::saveBlacklist(a));
	CHECK(Persist::loadBlacklist(b) && b.size() == 2 && b.contains("key-1") && b.contains("C-lDM7B2wGY7nURY"));
	// An empty list is stored too (every key re-enabled).
	a.clear();
	CHECK(Persist::saveBlacklist(a) && Persist::loadBlacklist(b) && b.size() == 0);
	// A damaged file is refused and leaves the set empty.
	a.upsert("key-2", 7000);
	CHECK(Persist::saveBlacklist(a));
	g_fs.files["/blacklist.bin"][10] ^= 0xFF;
	b.upsert("stale", 1);
	CHECK(!Persist::loadBlacklist(b) && b.size() == 0);
	// A full set round-trips through the shared staging buffer.
	BlacklistSet big, back;
	for (size_t i = 0; i < BlacklistSet::kCapacity; i++) {
		char id[16];
		snprintf(id, sizeof(id), "id%03zu", i);
		big.upsert(id, 9000 + (uint32_t)i);
	}
	CHECK(Persist::saveBlacklist(big) && Persist::loadBlacklist(back) && back.size() == BlacklistSet::kCapacity);
	CHECK(back.contains("id000") && back.contains("id199"));
}

static void testClockFile() {
	freshFlash();
	CHECK(Persist::loadClock() == 0);
	CHECK(Persist::saveClock(1767225600u) && Persist::loadClock() == 1767225600u);
	CHECK(Persist::saveClock(1767225700u) && Persist::loadClock() == 1767225700u);
	g_fs.files["/clock.bin"][5] ^= 1;
	CHECK(Persist::loadClock() == 0);
}

int main() {
	testJournalBasics();
	testShedDuringUpload();
	testDenialsNeverShedOpenings();
	testReplaceKeepsOldUntilRename();
	testTornTailRepair();
	testFailedWrites();
	testBlacklistFile();
	testClockFile();
	printf("%d persist checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
