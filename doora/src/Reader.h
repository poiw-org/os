// PN532 NFC reader. Hands out each presented tag once, with its UID and the key token stored in
// its first NDEF record.
//
// A task on core 1 (priority above loop()) polls the chip, so tag reads never wait for network
// work. It owns the one buffer the tag data is read into and publishes the result through a
// single slot: no copies, no queue, no heap. loop() takes the slot with take() and must give it
// back with release(); the reader task reads no further tag until then.
#pragma once
#include <Arduino.h>
#include <atomic>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "Jwt.h"
#include "Pn532.h"
#include "WriteState.h"

struct TagRead {
	uint8_t uid[10];
	uint8_t uidLen;
	const char* token;  // inside the reader's buffer; nullptr when the tag carries no key
	size_t tokenLen;
	uint32_t foundMs;  // millis() when the tag was detected, to time a tap end to end
};

class Reader {
 public:
	// Tag data area: the NDEF TLV holding a maximum-size token, rounded up to whole 16-byte reads.
	static constexpr size_t kAreaBytes = 896;

	// Probes the PN532 and starts the reader task. The door keeps running without the chip: the
	// task re-probes on its own, so other unlock methods are never blocked by the NFC hardware.
	void begin(uint8_t sda, uint8_t scl);
	// A tag that was not on the reader before, or nullptr. Valid until release().
	const TagRead* take() const { return full_.load(std::memory_order_acquire) ? &slot_ : nullptr; }
	void release() { full_.store(false, std::memory_order_release); }
	bool available() const { return ready_; }

	// Writer mode: the next tag with this UID gets `token` written to it (NDEF text record, as the
	// backoffice writes it with a phone), then is read back and compared. While armed no tag opens the
	// door. Returns false while a write is in progress or when the token does not fit a tag.
	bool arm(const char* token, size_t len, const uint8_t* uid, uint8_t uidLen, uint32_t windowMs);
	// Scan mode: same flow, but the next tag of any kind just has its UID reported (see scannedUid()).
	bool armScan(uint32_t windowMs);
	// The UID of the tag a finished scan saw.
	uint8_t scannedUid(uint8_t* out) const {
		memcpy(out, jobUid_, jobUidLen_);
		return jobUidLen_;
	}
	void cancelWrite();
	WriteState writeState() const { return writeState_.load(std::memory_order_acquire); }
	void printStatus() const;
	// Keeps the reader off the bus for `ms`: the denial chord plays right after a tap and a wedge
	// followed it, so the chip is left alone while it sounds.
	void holdOff(uint32_t ms) {
		holdUntilMs_.store(millis() + ms, std::memory_order_release);
		holding_.store(true, std::memory_order_release);
	}

 private:
	static void run(void* self);
	[[noreturn]] void loop();
	bool probe();
	void poll();
	void read(const Pn532::Target& target);
	void write(const Pn532::Target& target);
	WriteState writeTo(const Pn532::Target& target);
	WriteState writeType2(const Pn532::Target& target);
	WriteState writeClassic(const Pn532::Target& target);
	void finishWrite(WriteState s) { writeState_.store(s, std::memory_order_release); }
	// Moves an Armed or WrongTag session to `to`; false if it was not waiting (finished, being
	// rearmed by loop(), or being written). Every transition out of waiting goes through here.
	bool leaveWaiting(WriteState to);
	// Ends a waiting session whose window has passed; runs whether or not the chip answers.
	void checkDeadline();
	// Quiets the session so loop() may rewrite the job; false while a write is in progress.
	bool quiet();

	Pn532 pn_;
	TagRead slot_;
	std::atomic<bool> full_{false};
	volatile bool ready_ = false;
	uint8_t sda_ = 0, scl_ = 0;
	uint8_t errors_ = 0;
	bool present_ = false;  // a tag that was already handed out is still on the reader (reader task only)
	std::atomic<uint32_t> holdUntilMs_{0};
	std::atomic<bool> holding_{false};
	std::atomic<bool> rearm_{false};  // set by arm(): forget present_, the resting tag is the write target
	uint8_t retries_ = 0;   // reads of the present tag that failed on the radio link
	uint32_t nextProbeMs_ = 0;
	uint32_t lastSeenMs_ = 0;
	uint32_t lastCheckMs_ = 0;
	TaskHandle_t task_ = nullptr;
	uint8_t area_[kAreaBytes];

	// The write job: filled by arm() (loop task) before it publishes Armed, then used only by the reader task.
	std::atomic<WriteState> writeState_{WriteState::Idle};
	uint8_t job_[kAreaBytes];
	size_t jobLen_ = 0;
	uint8_t jobUid_[10];
	uint8_t jobUidLen_ = 0;
	bool scan_ = false;
	uint32_t jobDeadlineMs_ = 0;
};
