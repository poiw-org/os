// Runs the real Pn532.cpp against a simulated PN532 chip and tag. The I2C bus is the only seam:
// this file supplies SoftI2c's methods, so the protocol code is the production code. Time is
// virtual (stubs/Arduino.h). Build and run with `make test`.
#include <stdio.h>
#include <string.h>
#include <deque>
#include <vector>
#define private public  // the tests drive one poll at a time instead of starting the reader task
#include "../src/Reader.h"
#undef private
#include "../src/Pn532.h"
#include "../src/Pn532Frame.h"

uint64_t g_nowUs = 0;
HostSerial Serial;
static int failures = 0, checks = 0;
#define CHECK(cond)                                                       \
	do {                                                                  \
		checks++;                                                         \
		if (!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
	} while (0)

namespace {
struct Frame { std::vector<uint8_t> bytes; uint64_t readyAt; };

struct Chip {
	bool present = true;      // answers its I2C address
	bool silent = false;      // takes commands but never answers them
	bool corruptDcs = false;  // damages the data checksum of responses
	std::deque<Frame> out;    // frames waiting to be read, in order
	std::vector<uint8_t> in;  // bytes of the write transfer in progress
	bool writing = false, reading = false, firstReadDone = false;
	size_t pos = 0;
	bool statusReady = false;
	int aborts = 0, exchanges = 0;
	uint64_t freezeUs = 0;  // next "not ready" status poll is followed by this long a stall of the host CPU

	bool tagPresent = false;
	uint8_t sak = 0x08;
	std::vector<uint8_t> uid = {0x13, 0xE8, 0x1A, 0x11};
	std::vector<uint8_t> ats;
	uint8_t key[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};
	uint8_t keyB[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	bool authedB = false;
	bool classic = true;
	int authedSector = -1;
	uint8_t flat[4096];          // tag memory: Classic block b at b * 16, Type 2 page p at p * 4
	bool readOnly = false;       // refuses writes
	int failExchangeAfter = -1;  // from this many exchanges on, the tag stops answering (pulled away)
	bool fastRead = true;        // Type 2: supports FAST_READ (the original Ultralight does not)
	int lastPage = 230;          // Type 2: FAST_READ past this page is refused
	bool halted = false;         // refused a command; needs selecting again (InListPassiveTarget)
	int fastReads = 0;
} chip;

std::vector<uint8_t> makeFrame(uint8_t code, const std::vector<uint8_t>& data, bool corrupt) {
	std::vector<uint8_t> f = {0x00, 0x00, 0xFF, (uint8_t)(data.size() + 2), (uint8_t)(0x100 - (data.size() + 2)), 0xD5, code};
	uint8_t sum = (uint8_t)(0xD5 + code);
	for (uint8_t b : data) { f.push_back(b); sum = (uint8_t)(sum + b); }
	f.push_back((uint8_t)(0x100 - sum) ^ (corrupt ? 1 : 0));
	f.push_back(0x00);
	return f;
}

void enqueue(uint8_t code, const std::vector<uint8_t>& data) {
	chip.out.push_back({std::vector<uint8_t>(pn532::kAckFrame, pn532::kAckFrame + 6), g_nowUs + 300});
	chip.out.push_back({makeFrame(code, data, chip.corruptDcs), g_nowUs + 3000});  // RF work takes a few ms
}

void process() {
	const std::vector<uint8_t>& f = chip.in;
	if (f.size() == 6 && !memcmp(f.data(), pn532::kAckFrame, 6)) {  // host aborts the running command
		chip.out.clear();
		chip.aborts++;
		return;
	}
	if (f.size() < 8 || f[0] != 0 || f[1] != 0 || f[2] != 0xFF || f[5] != 0xD4 || f.size() != (size_t)f[3] + 7) return;
	uint8_t sum = 0;
	for (size_t i = 5; i + 1 < f.size(); i++) sum = (uint8_t)(sum + f[i]);
	if (sum != 0) return;  // bad host checksum: the chip ignores the frame
	if (chip.silent) return;
	uint8_t cmd = f[6];
	std::vector<uint8_t> arg(f.begin() + 7, f.end() - 2);
	switch (cmd) {
		case 0x02: enqueue(0x03, {0x32, 0x01, 0x06, 0x07}); break;
		case 0x14: enqueue(0x15, {}); break;
		case 0x32: enqueue(0x33, {}); break;
		case 0x4A:
			if (!chip.tagPresent) { enqueue(0x4B, {0}); break; }
			chip.halted = false;
			{
				std::vector<uint8_t> d = {1, 1, 0x00, 0x04, chip.sak, (uint8_t)chip.uid.size()};
				d.insert(d.end(), chip.uid.begin(), chip.uid.end());
				d.insert(d.end(), chip.ats.begin(), chip.ats.end());
				enqueue(0x4B, d);
			}
			break;
		case 0x40:    // InDataExchange: arg = {tg, tag command...}
		case 0x42: {  // InCommunicateThru: arg = {tag command...}
			chip.exchanges++;
			std::vector<uint8_t> t(arg.begin() + (cmd == 0x40 ? 1 : 0), arg.end());
			uint8_t status = chip.failExchangeAfter >= 0 && chip.exchanges > chip.failExchangeAfter ? 0x01 : 0x00;
			if (chip.halted) status = 0x01;  // a NAKed tag ignores everything until it is selected again
			std::vector<uint8_t> d;
			if (status) {
			} else if (t.size() >= 2 && (t[0] == 0x60 || t[0] == 0x61)) {  // AUTH_A / AUTH_B, block, key, uid
				bool b = t[0] == 0x61;
				bool ok = t.size() == 12 && !memcmp(&t[2], b ? chip.keyB : chip.key, 6) &&
				          !memcmp(&t[8], chip.uid.data() + chip.uid.size() - 4, 4);
				if (ok) {
					chip.authedSector = t[1] / 4;
					chip.authedB = b;
				} else status = 0x14;
			} else if (t.size() == 2 && t[0] == 0x30) {  // READ
				if (chip.classic && chip.authedSector != t[1] / 4) status = 0x01;
				else {
					const uint8_t* p = chip.flat + t[1] * (chip.classic ? 16 : 4);
					d.assign(p, p + 16);
				}
			} else if (t.size() == 3 && t[0] == 0x3A && !chip.classic) {  // FAST_READ first..last page
				chip.fastReads++;
				if (!chip.fastRead || t[2] < t[1] || t[2] > chip.lastPage) {
					status = 0x01;  // NAK: the tag halts
					chip.halted = true;
				} else {
					d.assign(chip.flat + t[1] * 4, chip.flat + (t[2] + 1) * 4);
				}
			} else if (t.size() == 6 && t[0] == 0xA2 && !chip.classic) {  // WRITE one page
				if (chip.readOnly) status = 0x01;
				else memcpy(chip.flat + t[1] * 4, &t[2], 4);
			} else if (t.size() == 18 && t[0] == 0xA0 && chip.classic) {  // WRITE one 16-byte block (needs key B)
				if (chip.authedSector != t[1] / 4 || !chip.authedB || chip.readOnly) status = 0x01;
				else memcpy(chip.flat + t[1] * 16, &t[2], 16);
			} else {
				status = 0x01;
			}
			if (status) d.clear();
			d.insert(d.begin(), status);
			enqueue(cmd + 1, d);
			break;
		}
	}
}
}  // namespace

// The bus, as the PN532 sees it.
void SoftI2c::begin(uint8_t sda, uint8_t scl) {
	sda_ = sda;
	scl_ = scl;
	stuck_ = false;
}
bool SoftI2c::sclRelease() { return true; }
void SoftI2c::recover() {}

bool SoftI2c::start(uint8_t addr7, bool read) {
	g_nowUs += 20;
	if (!chip.present || addr7 != 0x24) return false;
	chip.reading = read;
	chip.writing = !read;
	chip.pos = 0;
	chip.in.clear();
	if (read) chip.statusReady = !chip.out.empty() && chip.out.front().readyAt <= g_nowUs;
	return true;
}

bool SoftI2c::write(uint8_t b) {
	g_nowUs += 10;
	if (chip.writing) chip.in.push_back(b);
	return chip.writing;
}

uint8_t SoftI2c::read(bool) {
	g_nowUs += 10;
	if (!chip.reading) return 0xFF;
	size_t i = chip.pos++;
	if (i == 0) return chip.statusReady ? 1 : 0;
	if (!chip.statusReady || chip.out.empty()) return 0;
	const std::vector<uint8_t>& b = chip.out.front().bytes;
	return i - 1 < b.size() ? b[i - 1] : 0;
}

void SoftI2c::stop() {
	// A flash write on the other core freezes this one; time (and the chip) move on meanwhile.
	if (chip.reading && !chip.statusReady && chip.freezeUs) { g_nowUs += chip.freezeUs; chip.freezeUs = 0; }
	if (chip.writing) process();
	if (chip.reading && chip.statusReady && chip.pos > 1 && !chip.out.empty()) chip.out.pop_front();  // delivered
	chip.reading = chip.writing = false;
}

// Synthetic token (signed with the host tests' throwaway secret). Never paste a real key here: the repo is public.
static const char* TOKEN = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMTExMTExMS0yMjIyLTMzMzMtNDQ0NC01NTU1NTU1NTU1NTUiLCJqdGkiOiJhYWFhYWFhYS1iYmJiLWNjY2MtZGRkZC1lZWVlZWVlZWVlZWUiLCJhdWQiOiIwNDphMTpiMjpjMzpkNDplNTpmNiIsImlhdCI6MTcwMDAwMDAwMCwibmJmIjoxNzAwMDAwMTAwLCJleHAiOjE4MDAwMDAwMDB9.ZCcN-soRSAVt5aHA6L_ZcGh7UnjAbZbPvkSm1z1io4A";

// NDEF TLV with a text record holding the token, as a phone writes it (long form: it is over 254 bytes).
static size_t ndefBytes(uint8_t* a, const char* token) {
	size_t tl = strlen(token), payload = 3 + tl, msg = payload + 7, i = 0;
	a[i++] = 0x03; a[i++] = 0xFF; a[i++] = (uint8_t)(msg >> 8); a[i++] = (uint8_t)msg;
	a[i++] = 0xC1; a[i++] = 0x01; a[i++] = 0; a[i++] = 0; a[i++] = (uint8_t)(payload >> 8); a[i++] = (uint8_t)payload;
	a[i++] = 'T'; a[i++] = 0x02; a[i++] = 'e'; a[i++] = 'n';
	memcpy(a + i, token, tl);
	i += tl;
	a[i++] = 0xFE;
	return i;
}

static void resetChip();

// Lays the message over MIFARE Classic data blocks from block 4, sector trailers filled with junk.
static void writeClassic(const uint8_t* msg, size_t n) {
	for (int b = 4; b < 64; b++) memset(chip.flat + b * 16, b % 4 == 3 ? 0xEE : 0x00, 16);
	size_t at = 0;
	for (int b = 4; b < 64 && at < n; b++) {
		if (b % 4 == 3) continue;
		size_t c = n - at < 16 ? n - at : 16;
		memcpy(chip.flat + b * 16, msg + at, c);
		at += c;
	}
}

static void testReader() {
	static Reader reader;  // 900 bytes of tag buffer: keep it off the stack, as in the firmware
	uint8_t msg[512];
	size_t n = ndefBytes(msg, TOKEN);

	// MIFARE Classic tag, tapped: token read from the right blocks, once per sector authentication.
	resetChip();
	chip.tagPresent = true;
	writeClassic(msg, n);
	CHECK(reader.probe());
	reader.poll();
	const TagRead* tag = reader.take();
	CHECK(tag && tag->token && tag->tokenLen == strlen(TOKEN) && !memcmp(tag->token, TOKEN, tag->tokenLen));
	CHECK(tag && tag->uidLen == 4 && tag->uid[0] == 0x13 && tag->uid[3] == 0x11);
	CHECK(chip.exchanges <= 22 + 8);  // about 22 data blocks plus one authentication per sector, not all 60 blocks

	// The tag stays on the reader: not handed out again, even after release.
	reader.release();
	int seen = chip.exchanges;
	for (int i = 0; i < 5; i++) reader.poll();
	CHECK(!reader.take() && chip.exchanges == seen);

	// Removed for longer than the release time, then tapped again: read again.
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	chip.tagPresent = true;
	reader.poll();
	CHECK(reader.take() && reader.take()->token);
	reader.release();

	// Pulled away mid-read: the read stops at the first failure and is retried a bounded number of
	// times; then the tag is handed out with no token, so the denial is still journaled.
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	chip.tagPresent = true;
	chip.exchanges = 0;
	chip.failExchangeAfter = 5;
	reader.poll();
	CHECK(!reader.take() && chip.exchanges == 6);  // 5 good exchanges, then one failure, then stop
	for (int i = 0; i < 4 && !reader.take(); i++) reader.poll();
	CHECK(reader.take() && !reader.take()->token && chip.exchanges <= 6 + 2);
	reader.release();
	chip.failExchangeAfter = -1;

	// NTAG (Type 2, 7-byte UID): pages from 4, no authentication, four pages per read.
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	resetChip();
	chip.tagPresent = true;
	chip.classic = false;
	chip.sak = 0x00;
	chip.uid = {0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
	memset(chip.flat, 0, 16);
	memcpy(chip.flat + 16, msg, n);
	CHECK(reader.probe());
	reader.poll();
	tag = reader.take();
	CHECK(tag && tag->token && tag->tokenLen == strlen(TOKEN) && !memcmp(tag->token, TOKEN, tag->tokenLen));
	// FAST_READ: 4 pages for the TLV header, then the rest of the message in 60-page reads.
	size_t restPages = (n - 1 - 16 + 3) / 4;  // the terminator after the message is not read
	CHECK(tag && tag->uidLen == 7 && chip.exchanges == 1 + (int)((restPages + 59) / 60));
	CHECK(chip.exchanges < (int)((n + 15) / 16) / 4);  // READ alone needed one exchange per 16 bytes
	reader.release();

	// The original Ultralight has no FAST_READ: the NAK halts it, so it is selected again and read
	// with READ. Same token, one wasted exchange.
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	chip.fastRead = false;
	chip.exchanges = chip.fastReads = 0;
	chip.tagPresent = true;
	reader.poll();
	tag = reader.take();
	CHECK(tag && tag->token && tag->tokenLen == strlen(TOKEN) && !memcmp(tag->token, TOKEN, tag->tokenLen));
	CHECK(chip.fastReads == 1 && chip.exchanges == 1 + (int)((n - 1 + 15) / 16));
	reader.release();
	chip.fastRead = true;

	// A FAST_READ that fails on the link (tag at the edge of the field) is not a verdict: nothing is
	// handed out, and the next poll reads the tag again and finds the key.
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	chip.tagPresent = true;
	chip.exchanges = 0;
	chip.failExchangeAfter = 1;
	reader.poll();
	CHECK(!reader.take());
	chip.failExchangeAfter = -1;
	reader.poll();
	tag = reader.take();
	CHECK(tag && tag->token && tag->tokenLen == strlen(TOKEN));
	reader.release();

	// A tag that keeps failing is handed out without a token after the retries.
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	chip.tagPresent = true;
	chip.exchanges = 0;
	chip.failExchangeAfter = 0;
	for (int i = 0; i < 3; i++) {
		CHECK(!reader.take());
		reader.poll();
	}
	chip.failExchangeAfter = -1;
	tag = reader.take();
	CHECK(tag && !tag->token);
	reader.release();

	// The retry count goes with the tag: one that failed once, left and came back gets its retries
	// anew (a failed read clears present_, so leaving must reset the count regardless).
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	chip.tagPresent = true;
	chip.failExchangeAfter = 0;
	chip.exchanges = 0;
	reader.poll();
	CHECK(!reader.take());
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	chip.tagPresent = true;
	chip.exchanges = 0;
	reader.poll();
	CHECK(!reader.take());
	chip.exchanges = 0;
	reader.poll();
	CHECK(!reader.take());
	chip.failExchangeAfter = -1;
	reader.poll();
	tag = reader.take();
	CHECK(tag && tag->token && tag->tokenLen == strlen(TOKEN));
	reader.release();

	// Writer mode on the same NTAG: the matching tag gets the token and it reads back.
	reader.release();
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	memset(chip.flat, 0, sizeof(chip.flat));
	chip.flat[12] = 0xE1, chip.flat[13] = 0x10, chip.flat[14] = 0x6D;  // NTAG216 capability container
	const uint8_t uid7[7] = {0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
	const uint8_t other[7] = {0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
	CHECK(reader.arm(TOKEN, strlen(TOKEN), other, 7, 60000) && reader.writeState() == WriteState::Armed);
	chip.tagPresent = true;
	reader.poll();
	CHECK(reader.writeState() == WriteState::WrongTag && !reader.take());  // wrong tag: told so, door not opened
	reader.poll();
	CHECK(reader.writeState() == WriteState::WrongTag);
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	CHECK(reader.arm(TOKEN, strlen(TOKEN), uid7, 7, 60000));
	chip.tagPresent = true;
	reader.poll();
	CHECK(reader.writeState() == WriteState::Done && !reader.take());
	CHECK(!memcmp(chip.flat + 16, msg, n));
	chip.exchanges = 0;
	reader.poll();  // still resting on the reader: neither written again nor read as a tap
	CHECK(!reader.take() && chip.exchanges == 0);

	// Timeout, cancel, a tag too small, a tag that refuses, and a token that cannot be encoded.
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	CHECK(reader.arm(TOKEN, strlen(TOKEN), uid7, 7, 1000));
	g_nowUs += 2000000;
	reader.checkDeadline();
	CHECK(reader.writeState() == WriteState::Timeout);
	// The deadline holds while the chip is not answering too (it is checked outside poll()).
	CHECK(reader.arm(TOKEN, strlen(TOKEN), uid7, 7, 1000));
	reader.ready_ = false;
	g_nowUs += 2000000;
	reader.checkDeadline();
	CHECK(reader.writeState() == WriteState::Timeout);
	reader.ready_ = true;
	// A write in progress can be neither rearmed nor cancelled underneath the reader.
	reader.writeState_.store(WriteState::Writing);
	reader.cancelWrite();
	CHECK(reader.writeState() == WriteState::Writing);
	CHECK(!reader.arm(TOKEN, strlen(TOKEN), uid7, 7, 60000) && !reader.armScan(60000));
	g_nowUs += 120000000;
	reader.checkDeadline();
	CHECK(reader.writeState() == WriteState::Writing);  // only a waiting session times out
	reader.writeState_.store(WriteState::Failed);
	CHECK(reader.arm(TOKEN, strlen(TOKEN), uid7, 7, 60000));
	reader.cancelWrite();
	CHECK(reader.writeState() == WriteState::Cancelled);
	chip.flat[14] = 0x12;  // NTAG213: 144 bytes
	char big[600];
	memset(big, 'A', sizeof(big) - 1);
	big[sizeof(big) - 1] = 0;
	CHECK(reader.arm(big, strlen(big), uid7, 7, 60000));
	chip.tagPresent = true;
	reader.poll();
	CHECK(reader.writeState() == WriteState::TooSmall);
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	chip.flat[14] = 0x6D;
	chip.readOnly = true;
	CHECK(reader.arm(TOKEN, strlen(TOKEN), uid7, 7, 60000));
	chip.tagPresent = true;
	reader.poll();
	CHECK(reader.writeState() == WriteState::Failed);
	chip.readOnly = false;
	char huge[Reader::kAreaBytes + 1];
	memset(huge, 'A', sizeof(huge) - 1);
	huge[sizeof(huge) - 1] = 0;
	CHECK(!reader.arm(huge, strlen(huge), uid7, 7, 60000));
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	chip.tagPresent = true;
	chip.flat[12] = 0;  // not NDEF formatted
	reader.arm(TOKEN, strlen(TOKEN), uid7, 7, 60000);
	reader.poll();
	CHECK(reader.writeState() == WriteState::Unsupported);
	chip.flat[12] = 0xE1;
	reader.release();

	// MIFARE Classic: an NDEF-formatted card is written with key B and reads back; others are refused.
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	resetChip();
	chip.classic = true;
	chip.sak = 0x08;
	chip.uid = {0x13, 0xE8, 0x1A, 0x11};
	memset(chip.flat, 0, sizeof(chip.flat));
	CHECK(reader.arm(TOKEN, strlen(TOKEN), chip.uid.data(), 4, 60000));
	chip.tagPresent = true;
	reader.poll();
	CHECK(reader.writeState() == WriteState::Done && !reader.take());
	CHECK(!memcmp(chip.flat + 4 * 16, msg, 16) && !memcmp(chip.flat + 6 * 16, msg + 32, 16) &&
	      (n <= 48 || !memcmp(chip.flat + 8 * 16, msg + 48, 16)));  // blocks 4, 5, 6, then past the trailer to 8
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	// A card whose key A is not the NDEF key is refused untouched; a token over 45 blocks does not fit.
	chip.key[0] = 0x00;
	memset(chip.flat, 0, sizeof(chip.flat));
	CHECK(reader.arm(TOKEN, strlen(TOKEN), chip.uid.data(), 4, 60000));
	chip.tagPresent = true;
	reader.poll();
	CHECK(reader.writeState() == WriteState::Unsupported && chip.flat[4 * 16] == 0);
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	chip.key[0] = 0xD3;
	char large[Reader::kAreaBytes - 20];
	memset(large, 'A', sizeof(large) - 1);
	large[sizeof(large) - 1] = 0;
	CHECK(reader.arm(large, strlen(large), chip.uid.data(), 4, 60000));
	chip.tagPresent = true;
	reader.poll();
	CHECK(reader.writeState() == WriteState::TooSmall);
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();

	// A tag read before the session was armed is still held by loop(): its token points into the
	// buffer the write's read-back uses, so the write waits for release().
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	writeClassic(msg, n);
	chip.key[0] = 0xD3;
	chip.tagPresent = true;
	reader.poll();
	tag = reader.take();
	CHECK(tag && tag->token);
	char before[64];
	memcpy(before, tag->token, sizeof(before));
	CHECK(reader.arm(TOKEN, strlen(TOKEN), chip.uid.data(), 4, 60000));
	chip.exchanges = 0;
	reader.poll();
	reader.poll();
	CHECK(reader.writeState() == WriteState::Armed && chip.exchanges == 0 && !memcmp(tag->token, before, sizeof(before)));
	reader.release();
	reader.poll();
	CHECK(reader.writeState() == WriteState::Done && !reader.take());
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();

	// Scan mode reports any tag's UID and does not open the door.
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	CHECK(reader.armScan(60000));
	chip.tagPresent = true;
	reader.poll();
	uint8_t scanned[10];
	CHECK(reader.writeState() == WriteState::Done && !reader.take() && reader.scannedUid(scanned) == 4 &&
	      !memcmp(scanned, chip.uid.data(), 4));

	// Unsupported tag types (ISO-DEP only) are handed out without a token, not read.
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	chip.tagPresent = false;
	reader.poll();
	g_nowUs += 700000;
	reader.poll();
	chip.exchanges = 0;
	chip.sak = 0x20;
	chip.tagPresent = true;
	reader.poll();
	CHECK(reader.take() && !reader.take()->token && chip.exchanges == 0);
	reader.release();
}

static void resetChip() {
	Chip fresh;
	chip = fresh;
	for (int i = 0; i < (int)sizeof(chip.flat); i++) chip.flat[i] = (uint8_t)(i * 7 + 3);
}

int main() {
	Pn532 pn;
	uint8_t r[17];
	const uint8_t auth4[12] = {0x60, 4, 0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7, 0x13, 0xE8, 0x1A, 0x11};
	const uint8_t read4[2] = {0x30, 4};

	resetChip();
	CHECK(pn.begin(21, 22) && pn.version() == 0x32010607u);

	Pn532::Target t;
	CHECK(pn.detect(t) == Pn532::Detect::None);
	chip.tagPresent = true;
	CHECK(pn.detect(t) == Pn532::Detect::Found && t.uidLen == 4 && t.sak == 0x08 && !memcmp(t.uid, "\x13\xE8\x1A\x11", 4));

	// MIFARE Classic: reads need the sector authenticated first; a wrong key is refused, not a bus error.
	CHECK(pn.exchange(read4, 2, r, sizeof(r)) == Pn532::kErrTag);
	CHECK(pn.exchange(auth4, sizeof(auth4), r, sizeof(r)) == 0);
	CHECK(pn.exchange(read4, 2, r, sizeof(r)) == 16 && !memcmp(r, chip.flat + 64, 16));
	uint8_t badKey[12];
	memcpy(badKey, auth4, sizeof(badKey));
	badKey[2] ^= 0xFF;
	CHECK(pn.exchange(badKey, sizeof(badKey), r, sizeof(r)) == Pn532::kErrTag);

	// Type 2 tag, 7-byte UID, no authentication.
	chip.classic = false;
	chip.sak = 0x00;
	chip.uid = {0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
	CHECK(pn.detect(t) == Pn532::Detect::Found && t.uidLen == 7 && t.sak == 0x00);
	CHECK(pn.exchange(read4, 2, r, sizeof(r)) == 16 && !memcmp(r, chip.flat + 16, 16));
	// FAST_READ through InCommunicateThru: 60 pages fill the largest normal frame the chip sends.
	const uint8_t fast60[3] = {0x3A, 4, 63};
	uint8_t big[241];
	CHECK(pn.communicate(fast60, sizeof(fast60), big, sizeof(big)) == 240 && !memcmp(big, chip.flat + 16, 240));
	CHECK(pn.communicate(fast60, sizeof(fast60), big, 100) == Pn532::kErrFrame);  // does not fit the caller's buffer

	// An ISO-DEP tag appends a long ATS after the UID; that must not look like a bus error.
	chip.ats.assign(40, 0x77);
	chip.sak = 0x20;
	CHECK(pn.detect(t) == Pn532::Detect::Found && t.sak == 0x20);
	// Even one that pushes the answer far past 64 bytes (an emulator can send any ATS).
	chip.ats.assign(200, 0x77);
	CHECK(pn.detect(t) == Pn532::Detect::Found && t.sak == 0x20 && t.uidLen == 7);
	chip.ats.clear();

	// A response with a damaged checksum is rejected as a frame error, and the next command works.
	chip.corruptDcs = true;
	CHECK(pn.exchange(read4, 2, r, sizeof(r)) == Pn532::kErrFrame);
	chip.corruptDcs = false;
	CHECK(pn.exchange(read4, 2, r, sizeof(r)) == 16);

	// The host is frozen past every deadline right after a "not ready" poll (a flash write on the other
	// core): the answer that arrived meanwhile still counts, and the chip is not aborted mid-command.
	int abortsBefore = chip.aborts;
	chip.freezeUs = 400000;
	CHECK(pn.detect(t) == Pn532::Detect::Found);
	CHECK(chip.freezeUs == 0 && chip.aborts == abortsBefore);

	// A chip that stops answering costs a bounded wait, is told to abort, and recovers afterwards.
	chip.silent = true;
	uint64_t before = g_nowUs;
	CHECK(pn.exchange(read4, 2, r, sizeof(r)) == Pn532::kErrTimeout);
	CHECK(g_nowUs - before < 200000 && chip.aborts >= 1);
	chip.silent = false;
	CHECK(pn.detect(t) == Pn532::Detect::Found);
	CHECK(pn.exchange(read4, 2, r, sizeof(r)) == 16);

	// No chip on the bus: begin() reports it and detect() reports an error instead of hanging.
	chip.present = false;
	before = g_nowUs;
	CHECK(!pn.begin(21, 22));
	CHECK(pn.detect(t) == Pn532::Detect::Error);
	CHECK(g_nowUs - before < 500000);

	testReader();
	printf("%d chip checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
