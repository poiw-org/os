#include "RemoteConsole.h"
#include <Arduino.h>
#include <freertos/ringbuf.h>
#include <string.h>
#include <atomic>

namespace {
constexpr size_t kBufferBytes = 2048;
constexpr uint32_t kLeaseMs = 30000;

RingbufHandle_t gBuffer = nullptr;
std::atomic<uint32_t> gLeaseUntil{0};
std::atomic<bool> gHeld{false}, gMuted{false};
}  // namespace

namespace RemoteConsole {

void begin() { gBuffer = xRingbufferCreate(kBufferBytes, RINGBUF_TYPE_BYTEBUF); }

void renewLease() {
	gLeaseUntil.store(millis() + kLeaseMs);
	gHeld.store(true);
}

bool active() {
	if (!gHeld.load()) return false;
	if ((int32_t)(millis() - gLeaseUntil.load()) < 0) return true;
	gHeld.store(false);
	return false;
}

void write(const uint8_t* data, size_t len) {
	if (gBuffer && !gMuted.load() && active()) xRingbufferSend(gBuffer, data, len, 0);
}

size_t read(char* out, size_t max) {
	if (!gBuffer) return 0;
	size_t n = 0;
	auto* item = static_cast<char*>(xRingbufferReceiveUpTo(gBuffer, &n, 0, max));
	if (!item) return 0;
	memcpy(out, item, n);
	vRingbufferReturnItem(gBuffer, item);
	return n;
}

void mute(bool on) { gMuted.store(on); }

}  // namespace RemoteConsole
