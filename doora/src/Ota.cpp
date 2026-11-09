#include "Ota.h"
#include "Log.h"
#include "Pins.h"  // config.h

#ifdef OTA_SIGNING_KEY
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include "OtaImage.h"

namespace {
constexpr uint8_t kSigningKey[kRsaBytes] = OTA_SIGNING_KEY;
constexpr uint8_t kAck = 0x06;
constexpr size_t kChunk = 1024;  // must fit Bt.cpp's receive buffer; tools/bt_update.py sends the same
constexpr uint32_t kStallMs = 10000;

RsaPublicKey gKey;
bool gReady = false;

bool readPartition(void* ctx, size_t offset, uint8_t* out, size_t len) {
	return esp_partition_read(static_cast<const esp_partition_t*>(ctx), offset, out, len) == ESP_OK;
}

// Waits for `want` bytes; false after kStallMs without any.
bool fill(uint8_t* out, size_t want, void (*idle)()) {
	size_t n = 0;
	uint32_t last = millis();
	while (n < want) {
		if (idle) idle();
		if (!Term.available()) {
			if (millis() - last > kStallMs) return false;
			delay(1);
			continue;
		}
		while (n < want && Term.available()) out[n++] = (uint8_t)Term.read();
		last = millis();
	}
	return true;
}
}  // namespace

void Ota::begin() {
	gReady = rsaPrepare(kSigningKey, gKey);
	if (!gReady) LOG("OTA: OTA_SIGNING_KEY in config.h is not a 2048-bit RSA modulus; updates stay off.");
}

void Ota::receive(size_t total, void (*idle)()) {
	const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
	esp_ota_handle_t handle = 0;
	const char* why = nullptr;
	if (!gReady) why = "no signing key";
	else if (!next || total <= kOtaTrailer || total > next->size) why = "bad size";
	else if (esp_ota_begin(next, OTA_WITH_SEQUENTIAL_WRITES, &handle) != ESP_OK) why = "could not start";
	if (why) {
		LOG("UPDATE FAILED: %s.", why);
		return;
	}

	LOG("OTA: receiving %u bytes.", (unsigned)total);
	Term.write(kAck);
	uint8_t chunk[kChunk];
	for (size_t got = 0; got < total;) {
		size_t want = total - got < kChunk ? total - got : kChunk;
		if (!fill(chunk, want, idle)) { why = "the upload stalled"; break; }
		if (esp_ota_write(handle, chunk, want) != ESP_OK) { why = "flash write failed"; break; }
		got += want;
		Term.write(kAck);
	}
	if (why) {
		esp_ota_abort(handle);
		LOG("UPDATE FAILED: %s; keeping the running firmware.", why);
		return;
	}
	if (esp_ota_end(handle) != ESP_OK) why = "the image is corrupt";
	else if (!otaImageSigned(gKey, readPartition, (void*)next, total)) why = "the image is not signed with this door's key";
	else if (esp_ota_set_boot_partition(next) != ESP_OK) why = "could not activate it";
	if (why) {
		LOG("UPDATE FAILED: %s; keeping the running firmware.", why);
		return;
	}
	LOG("UPDATE OK; restarting.");
	delay(500);  // lets the line reach the client
	ESP.restart();
}

#else

void Ota::begin() { LOG("OTA: off (set OTA_SIGNING_KEY in config.h)."); }
void Ota::receive(size_t, void (*)()) { LOG("UPDATE FAILED: this build has no signing key."); }

#endif
