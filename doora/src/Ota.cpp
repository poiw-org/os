#include "Ota.h"
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <atomic>
#include "Log.h"
#include "Sha256.h"

// Arduino confirms every image at startup unless told the application will do it.
extern "C" bool verifyRollbackLater() { return true; }

namespace {
constexpr uint32_t kTrialMs = 600000;
constexpr const char* kPrefs = "ota";
constexpr const char* kReleaseKey = "release";

esp_ota_handle_t gHandle = 0;
const esp_partition_t* gTarget = nullptr;
Sha256 gHash;
const char* gError = "";
std::atomic<bool> gRestart{false};

// Set in begin(), read by the REST task.
char gRelease[40] = {0};
std::atomic<bool> gTrial{false}, gRolledBack{false};

void storeRelease(const char* id) {
	Preferences p;
	p.begin(kPrefs, false);
	if (id) p.putString(kReleaseKey, id);
	else p.remove(kReleaseKey);
	p.end();
}

bool fail(const char* why) {
	gError = why;
	return false;
}
}  // namespace

void Ota::begin() {
	Preferences p;
	p.begin(kPrefs, true);
	p.getString(kReleaseKey, gRelease, sizeof(gRelease));
	p.end();

	esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
	esp_ota_get_state_partition(esp_ota_get_running_partition(), &state);
	if (state == ESP_OTA_IMG_PENDING_VERIFY) {
		gTrial = true;
		LOG("OTA: running on trial until the door is back online.");
	} else if (gRelease[0]) {
		gRolledBack = true;  // an update was recorded, yet this is not the image on trial
		LOG("OTA: the last update failed its trial; back on the previous firmware.");
	}
}

void Ota::service(bool idle) {
	if (gTrial && millis() > kTrialMs) {
		LOG("OTA: the door did not get back online; going back to the previous firmware.");
		delay(100);
		esp_ota_mark_app_invalid_rollback_and_reboot();
	}
	if (gRestart && idle) {
		LOG("OTA: restarting into the new firmware.");
		delay(100);
		ESP.restart();
	}
}

bool Ota::open(size_t total) {
	gTarget = esp_ota_get_next_update_partition(nullptr);
	if (!gTarget || !total || total > gTarget->size) return fail("the image does not fit");
	if (esp_ota_begin(gTarget, OTA_WITH_SEQUENTIAL_WRITES, &gHandle) != ESP_OK) return fail("could not start");
	gHash.reset();
	return true;
}

bool Ota::write(const char* data, size_t len) {
	gHash.update(reinterpret_cast<const uint8_t*>(data), len);
	return esp_ota_write(gHandle, data, len) == ESP_OK || fail("flash write failed");
}

void Ota::abort() { esp_ota_abort(gHandle); }

bool Ota::finish(const char* expectedSha256, const char* releaseId) {
	uint8_t digest[32];
	gHash.final(digest);
	char hex[65];
	for (size_t i = 0; i < sizeof(digest); i++) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
	if (strcmp(hex, expectedSha256) != 0) {
		esp_ota_abort(gHandle);
		return fail("the download does not match the approved hash");
	}
	if (esp_ota_end(gHandle) != ESP_OK) return fail("the image is corrupt");
	storeRelease(releaseId);
	if (esp_ota_set_boot_partition(gTarget) != ESP_OK) {
		storeRelease(nullptr);
		return fail("could not activate the image");
	}
	gRestart = true;
	return true;
}

const char* Ota::error() { return gError; }

const char* Ota::trialRelease() { return gTrial ? gRelease : nullptr; }

void Ota::confirm() {
	esp_ota_mark_app_valid_cancel_rollback();
	storeRelease(nullptr);
	gTrial = false;
	LOG("OTA: update confirmed.");
}

const char* Ota::rolledBackRelease() { return gRolledBack ? gRelease : nullptr; }

void Ota::clearRolledBack() {
	storeRelease(nullptr);
	gRolledBack = false;
}
