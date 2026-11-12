#include "Net.h"
#include <Arduino.h>
#include <atomic>
#include <lwip/sockets.h>
#include <mutex>
#include "Certs.h"
#include "Log.h"

namespace {
// Each TLS session keeps about 40 KB of buffers for as long as it lives (the 16 KB records of
// the precompiled mbedTLS, one per direction) and a handshake needs roughly 20 KB more. With the
// websocket up and a REST call starting, these two limits keep a connection attempt from using
// the memory Wi-Fi and the other tasks need.
constexpr uint32_t kMinFreeHeap = 72000;
constexpr uint32_t kMinBlock = 24000;  // the 16 KB record buffers must each fit in one block

std::mutex gHandshake;
std::atomic<uint32_t> gRefused{0};
}  // namespace

namespace Net {

bool roomForSession() { return ESP.getFreeHeap() >= kMinFreeHeap && ESP.getMaxAllocHeap() >= kMinBlock; }

bool connect(WiFiClientSecure& tls, const char* host, uint32_t timeoutMs) {
	tls.stop();
	std::lock_guard<std::mutex> oneAtATime(gHandshake);
	if (!roomForSession()) {
		LOG("Not enough memory for TLS (%lu bytes free, largest block %lu, %lu refusals).",
		    (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap(), (unsigned long)++gRefused);
		return false;
	}
	tls.setCACert(kRootCerts);  // certificate validity is checked against the system time, which Clock keeps set
	tls.setHandshakeTimeout((timeoutMs + 999) / 1000);  // the library's default would be 120 s
	if (!tls.connect(host, 443, (int32_t)timeoutMs)) {
		char err[80] = "";
		tls.lastError(err, sizeof(err));
		LOG("TLS connect to %s failed (%s; heap %lu free, lowest %lu, largest block %lu).", host, err,
		    (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
		return false;
	}
	tls.setNoDelay(true);
	return true;
}

bool waitReadable(WiFiClientSecure& tls, uint32_t ms) {
	if (tls.available() > 0) return true;  // already decrypted and waiting
	// Every false return must have waited: the callers loop on this until their deadline, and one
	// that came back at once would spin the task and starve the idle task into the watchdog (seen
	// when an OTA upload left lwIP short of buffers and select() failed).
	auto timedOut = [ms]() {
		vTaskDelay(pdMS_TO_TICKS(ms) ? pdMS_TO_TICKS(ms) : 1);
		return false;
	};
	int fd = tls.fd();
	if (fd < 0) return timedOut();
	fd_set readable;
	FD_ZERO(&readable);
	FD_SET(fd, &readable);
	timeval limit = {(time_t)(ms / 1000), (suseconds_t)((ms % 1000) * 1000)};
	int ready = select(fd + 1, &readable, nullptr, nullptr, &limit);
	if (ready < 0) return timedOut();
	if (ready == 0) return false;  // select() waited the full time
	if (tls.available() > 0) return true;  // reading the record decrypts it
	// Readable yet nothing to deliver: a partial record (the next select waits for the rest), or
	// the peer closed. A closed socket stays readable forever, so end it here instead of spinning.
	char probe;
	if (tls.connected() && recv(fd, &probe, 1, MSG_PEEK | MSG_DONTWAIT) == 0) tls.stop();
	vTaskDelay(1);
	return false;
}

uint32_t lowMemoryRefusals() { return gRefused.load(); }

}  // namespace Net
