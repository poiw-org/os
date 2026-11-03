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
#include <stddef.h>
#include <stdint.h>
#include "Jwt.h"
#include "Pn532.h"

struct TagRead {
	uint8_t uid[10];
	uint8_t uidLen;
	const char* token;  // inside the reader's buffer; nullptr when the tag carries no key
	size_t tokenLen;
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
	void printStatus() const;

 private:
	static void run(void* self);
	[[noreturn]] void loop();
	bool probe();
	void poll();
	void read(const Pn532::Target& target);

	Pn532 pn_;
	TagRead slot_;
	std::atomic<bool> full_{false};
	volatile bool ready_ = false;
	uint8_t sda_ = 0, scl_ = 0;
	uint8_t errors_ = 0;
	bool present_ = false;  // a tag that was already handed out is still on the reader
	uint32_t lastSeenMs_ = 0;
	uint32_t lastCheckMs_ = 0;
	TaskHandle_t task_ = nullptr;
	uint8_t area_[kAreaBytes];
};
