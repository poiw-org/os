#include "Pn532.h"
#include <Arduino.h>
#include <string.h>
#include "Pn532Frame.h"

namespace {
constexpr uint8_t kAddr = 0x24;  // 7-bit; the datasheet's 0x48 / 0x49 include the R/W bit

constexpr uint8_t kCmdFirmware = 0x02;
constexpr uint8_t kCmdRfConfig = 0x32;
constexpr uint8_t kCmdSam = 0x14;
constexpr uint8_t kCmdListTarget = 0x4A;
constexpr uint8_t kCmdExchange = 0x40;
constexpr uint8_t kCmdThrough = 0x42;

constexpr uint32_t kAckMs = 50;
constexpr uint32_t kConfigMs = 50;
constexpr uint32_t kDetectMs = 250;  // the chip gives up on its own after kDetectRetries attempts
constexpr uint32_t kExchangeMs = 100;
// Poll the ready flag back to back for this long (a tag exchange takes about this), then sleep a
// tick between polls so a stuck chip never starves the other tasks.
constexpr uint32_t kSpinUs = 1500;
// Activation attempts per detect(): 0xFF would mean "forever".
constexpr uint8_t kDetectRetries = 2;
constexpr size_t kMaxPayload = 40;
// abort() reads out at most this many leftover frames; a command leaves two (ACK and response).
constexpr int kMaxQueuedFrames = 4;
constexpr int kBeginAttempts = 3;
}  // namespace

// Polls the status byte until the chip has something to say. On success the read transaction is
// left open, positioned just after the status byte.
bool Pn532::waitReady(uint32_t timeoutMs) {
	uint32_t startMs = millis();
	uint32_t spinUntilUs = micros() + kSpinUs;
	for (;;) {
		// The deadline is read before the poll, so the chip is always asked once more after it has
		// passed: a flash write elsewhere can freeze this core for longer than a timeout, and the
		// answer that arrived meanwhile must not be thrown away (and the chip aborted mid-command).
		bool late = millis() - startMs >= timeoutMs;
		if (i2c_.start(kAddr, true)) {
			if (i2c_.read(true) & 1) return true;
			i2c_.read(false);  // not ready: finish the transfer
			i2c_.stop();
		}
		if (i2c_.stuck() || late) return false;
		if ((int32_t)(micros() - spinUntilUs) < 0) delayMicroseconds(100);
		else vTaskDelay(1);
	}
}

bool Pn532::writeFrame(const uint8_t* payload, size_t n) {
	if (n > kMaxPayload) return false;
	uint8_t frame[kMaxPayload + 8];
	size_t size = pn532::buildFrame(frame, payload, n);
	// A chip that was asleep ignores its first address byte.
	for (int attempt = 0; attempt < 3; attempt++) {
		if (!i2c_.start(kAddr, false)) { delay(1); continue; }
		bool ok = true;
		for (size_t i = 0; i < size && ok; i++) ok = i2c_.write(frame[i]);
		i2c_.stop();
		return ok;
	}
	return false;
}

bool Pn532::readAck() {
	if (!waitReady(kAckMs)) return false;
	uint8_t got[sizeof(pn532::kAckFrame)];
	for (size_t i = 0; i < sizeof(got); i++) got[i] = i2c_.read(i + 1 < sizeof(got));
	i2c_.stop();
	if (i2c_.stuck()) return false;
	return memcmp(got, pn532::kAckFrame, sizeof(got)) == 0;
}

// Reads the response frame of command `cmd` (the transaction is open after the status byte) and
// returns the length of its data, which is copied to `resp`.
int Pn532::readResponse(uint8_t cmd, uint8_t* resp, size_t cap) {
	uint8_t head[5];
	for (uint8_t& b : head) b = i2c_.read(true);
	if (i2c_.stuck()) return kErrBus;  // every further byte would cost a full clock-stretch timeout
	size_t len = head[3];  // TFI + response code + data
	if (!pn532::headerValid(head) || len < 2) {  // includes the chip's 1-byte application error frame
		i2c_.read(false);
		i2c_.stop();
		return kErrFrame;
	}
	uint8_t tfi = i2c_.read(true);
	uint8_t code = i2c_.read(true);
	uint8_t sum = (uint8_t)(tfi + code);
	size_t dataLen = len - 2;
	for (size_t i = 0; i < dataLen; i++) {
		uint8_t b = i2c_.read(true);
		if (i2c_.stuck()) return kErrBus;
		sum = (uint8_t)(sum + b);
		if (i < cap) resp[i] = b;
	}
	uint8_t dcs = i2c_.read(true);
	i2c_.read(false);  // postamble; the NACK ends the transfer
	i2c_.stop();
	if (tfi != pn532::kTfiFromChip || code != cmd + 1 || (uint8_t)(sum + dcs) != 0 || dataLen > cap) return kErrFrame;
	return (int)dataLen;
}

// Cancels the command the chip is running (an ACK frame from the host does that) and discards
// every frame it had already queued, so the next command starts from a quiet chip.
void Pn532::abort() {
	if (i2c_.start(kAddr, false)) {
		for (uint8_t b : pn532::kAckFrame) i2c_.write(b);
		i2c_.stop();
	}
	for (int frame = 0; frame < kMaxQueuedFrames; frame++) {
		delay(2);
		if (!i2c_.start(kAddr, true)) return;
		bool ready = i2c_.read(true) & 1;
		for (int i = 0; ready && i < 31; i++) i2c_.read(true);
		i2c_.read(false);
		i2c_.stop();
		if (!ready) return;
	}
}

int Pn532::command(const uint8_t* payload, size_t n, uint8_t* resp, size_t cap, uint32_t timeoutMs) {
	if (!writeFrame(payload, n)) return kErrBus;
	if (!readAck()) {
		int err = failure();
		abort();
		return err;
	}
	if (!waitReady(timeoutMs)) {
		int err = failure();
		abort();
		return err;
	}
	return readResponse(payload[0], resp, cap);
}

uint32_t Pn532::firmware() {
	const uint8_t cmd[] = {kCmdFirmware};
	uint8_t r[4];
	if (command(cmd, sizeof(cmd), r, sizeof(r), kConfigMs) != 4 || r[0] != 0x32) return 0;  // 0x32 = PN532
	return (uint32_t)r[0] << 24 | (uint32_t)r[1] << 16 | (uint32_t)r[2] << 8 | r[3];
}

bool Pn532::ping() { return firmware() != 0; }

bool Pn532::begin(uint8_t sda, uint8_t scl) {
	for (int attempt = 0; attempt < kBeginAttempts; attempt++) {
		if (attempt) delay(40);  // the chip needs a moment after power-up
		i2c_.begin(sda, scl);
		// A soft reboot does not reset the chip, which still holds whatever the previous run left
		// unread. Read as answers, those frames shift every later response by one command.
		abort();
		if (configure()) return true;
	}
	return false;
}

bool Pn532::configure() {
	version_ = firmware();
	if (!version_) return false;
	uint8_t r[4];
	const uint8_t sam[] = {kCmdSam, 0x01, 0x14, 0x00};  // normal mode, IRQ pin unused
	// Item 5, MaxRetries: ATR 0xFF and PSL 0x01 are the defaults; the last byte bounds detect().
	const uint8_t retries[] = {kCmdRfConfig, 0x05, 0xFF, 0x01, kDetectRetries};
	return command(sam, sizeof(sam), r, sizeof(r), kConfigMs) >= 0 &&
	       command(retries, sizeof(retries), r, sizeof(r), kConfigMs) >= 0;
}

Pn532::Detect Pn532::detect(Target& out) {
	const uint8_t cmd[] = {kCmdListTarget, 0x01, 0x00};  // one target, 106 kbps type A
	// ISO-DEP tags (phones, DESFire, emulators) append their ATS after the UID, up to a full normal
	// frame: an answer that does not fit would read as a bus error and keep resetting the chip.
	uint8_t r[kMaxThroughData + 1];
	int n = command(cmd, sizeof(cmd), r, sizeof(r), kDetectMs);
	lastError_ = n;
	if (n < 0) return Detect::Error;
	if (n < 1 || r[0] == 0) return Detect::None;
	// NbTg, Tg, SENS_RES (2), SEL_RES, NFCID length, NFCID...
	if (n < 6 || r[5] > sizeof(out.uid) || n < 6 + r[5]) { lastError_ = kErrFrame; return Detect::Error; }
	out.sak = r[4];
	out.uidLen = r[5];
	memcpy(out.uid, r + 6, out.uidLen);
	if (!out.uidLen) lastError_ = kErrFrame;
	return out.uidLen ? Detect::Found : Detect::Error;
}

int Pn532::exchange(const uint8_t* data, size_t n, uint8_t* out, size_t cap) {
	return transceive(kCmdExchange, true, data, n, out, cap);
}

int Pn532::communicate(const uint8_t* data, size_t n, uint8_t* out, size_t cap) {
	return transceive(kCmdThrough, false, data, n, out, cap);
}

int Pn532::transceive(uint8_t code, bool withTarget, const uint8_t* data, size_t n, uint8_t* out, size_t cap) {
	uint8_t cmd[24];
	size_t head = withTarget ? 2 : 1;
	if (n + head > sizeof(cmd)) return kErrFrame;
	cmd[0] = code;
	if (withTarget) cmd[1] = 0x01;  // target number
	memcpy(cmd + head, data, n);
	int got = command(cmd, n + head, out, cap, kExchangeMs);
	if (got < 1) return got < 0 ? got : kErrFrame;
	if (out[0] & 0x3F) return kErrTag;  // status byte: low 6 bits are the error code
	memmove(out, out + 1, (size_t)got - 1);
	return got - 1;
}
