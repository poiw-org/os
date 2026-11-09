#include "Ota.h"
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <atomic>
#include "Log.h"
#include "OtaImage.h"
#include "Pins.h"  // config.h

// Arduino confirms every image at startup unless told the application will do it.
extern "C" bool verifyRollbackLater() { return true; }

namespace {
constexpr uint32_t kTrialMs = 600000;
constexpr const char* kPrefs = "ota";
constexpr const char* kReleaseKey = "release";
#ifdef OTA_SIGNING_KEY
constexpr uint8_t kSigningKey[kRsaBytes] = OTA_SIGNING_KEY;
#endif

RsaPublicKey gKey;
bool gReady = false;
esp_ota_handle_t gHandle = 0;
const esp_partition_t* gTarget = nullptr;
const char* gError = "";
std::atomic<bool> gRestart{false};

// Set in begin(), read by the REST task.
char gRelease[40] = {0};
std::atomic<bool> gTrial{false}, gRolledBack{false};

bool readPartition(void* ctx, size_t offset, uint8_t* out, size_t len) {
	return esp_partition_read(static_cast<const esp_partition_t*>(ctx), offset, out, len) == ESP_OK;
}

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
#ifdef OTA_SIGNING_KEY
	gReady = rsaPrepare(kSigningKey, gKey);
	if (!gReady) LOG("OTA: OTA_SIGNING_KEY in config.h is not a 2048-bit RSA modulus; updates stay off.");
#else
	LOG("OTA: off (set OTA_SIGNING_KEY in config.h).");
#endif
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
	if (!gReady) return fail("this door has no signing key");
	gTarget = esp_ota_get_next_update_partition(nullptr);
	if (!gTarget || total <= kOtaTrailer || total > gTarget->size) return fail("the image does not fit");
	if (esp_ota_begin(gTarget, OTA_WITH_SEQUENTIAL_WRITES, &gHandle) != ESP_OK) return fail("could not start");
	return true;
}

bool Ota::write(const char* data, size_t len) {
	return esp_ota_write(gHandle, data, len) == ESP_OK || fail("flash write failed");
}

void Ota::abort() { esp_ota_abort(gHandle); }

bool Ota::finish(size_t total, const char* releaseId) {
	if (esp_ota_end(gHandle) != ESP_OK) return fail("the image is corrupt");
	if (!otaImageSigned(gKey, readPartition, (void*)gTarget, total)) return fail("the image is not signed with this door's key");
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
