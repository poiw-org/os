#include "Ota.h"
#include "Log.h"
#include "Pins.h"  // config.h

#if defined(OTA_PASSWORD_MD5) && defined(OTA_SIGNING_KEY)
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include "OtaImage.h"

namespace {
constexpr uint8_t kSigningKey[kRsaBytes] = OTA_SIGNING_KEY;
constexpr uint32_t kPollMs = 50;
// The idle priority: ArduinoOTA's receive loop only sleeps when no data is waiting, so during an
// upload it would keep core 0 busy for seconds. Sharing time with the idle task keeps the task
// watchdog fed (it reset the door mid-upload at priority 1), and every other task still preempts it.
constexpr UBaseType_t kTaskPriority = tskIDLE_PRIORITY;
constexpr BaseType_t kTaskCore = 0;
constexpr uint32_t kTaskStack = 6144;

RsaPublicKey gKey;
char gHostname[48];
size_t gTotal = 0;
bool gRefused = false;

bool readPartition(void* ctx, size_t offset, uint8_t* out, size_t len) {
	return esp_partition_read(static_cast<const esp_partition_t*>(ctx), offset, out, len) == ESP_OK;
}

void onStart() {
	gTotal = 0;
	gRefused = ArduinoOTA.getCommand() != U_FLASH;
	if (gRefused) {
		LOG("OTA: refused a filesystem upload; only signed firmware is accepted.");
		Update.abort();  // nothing has been written yet
		return;
	}
	LOG("OTA: receiving firmware...");
}

void onProgress(unsigned int done, unsigned int total) {
	gTotal = total;
	(void)done;
}

// Runs after Update.end(), which has already made the new image the boot partition. Undo that at
// once, and only point the boot partition at the new image when its signature checks out. A
// rejected image also loses its first sector: then it cannot boot whatever the OTA data says (the
// boot loader falls back to the other slot), so a failed switch-back can never start it.
void onEnd() {
	const esp_partition_t* running = esp_ota_get_running_partition();
	const esp_partition_t* fresh = esp_ota_get_boot_partition();
	esp_err_t back = esp_ota_set_boot_partition(running);
	if (!fresh || fresh == running || !otaImageSigned(gKey, readPartition, (void*)fresh, gTotal)) {
		esp_err_t wiped = fresh && fresh != running ? esp_partition_erase_range(fresh, 0, SPI_FLASH_SEC_SIZE) : ESP_OK;
		LOG("OTA: the image is not signed with this door's key; discarded, keeping the running firmware "
		    "(switch back: %s, erase: %s).", esp_err_to_name(back), esp_err_to_name(wiped));
		return;
	}
	if (esp_ota_set_boot_partition(fresh) != ESP_OK) {
		LOG("OTA: could not activate the new firmware; keeping the running one.");
		return;
	}
	LOG("OTA: signed firmware installed (%u bytes); restarting.", (unsigned)gTotal);
	delay(200);
	ESP.restart();
}

void onError(ota_error_t e) {
	if (!gRefused) LOG("OTA: update failed (error %u); keeping the running firmware.", (unsigned)e);
}

[[noreturn]] void run(void*) {
	while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));
	ArduinoOTA.setHostname(gHostname);
	ArduinoOTA.setPasswordHash(OTA_PASSWORD_MD5);
	ArduinoOTA.setRebootOnSuccess(false);  // onEnd() restarts, and only for a signed image
	ArduinoOTA.onStart(onStart);
	ArduinoOTA.onProgress(onProgress);
	ArduinoOTA.onEnd(onEnd);
	ArduinoOTA.onError(onError);
	uint32_t before = ESP.getFreeHeap();
	ArduinoOTA.begin();
	LOG("OTA: listening as %s.local (listener uses %ld bytes of heap, plus a %u byte task stack).", gHostname,
	    (long)before - (long)ESP.getFreeHeap(), (unsigned)kTaskStack);
	for (;;) {
		ArduinoOTA.handle();
		vTaskDelay(pdMS_TO_TICKS(kPollMs));
	}
}
}  // namespace

void Ota::begin(const char* hostname) {
	if (!rsaPrepare(kSigningKey, gKey)) {
		LOG("OTA: OTA_SIGNING_KEY in config.h is not a 2048-bit RSA modulus; OTA stays off.");
		return;
	}
	strlcpy(gHostname, hostname, sizeof(gHostname));
	xTaskCreatePinnedToCore(&run, "ota", kTaskStack, nullptr, kTaskPriority, nullptr, kTaskCore);
}

#else

void Ota::begin(const char*) { LOG("OTA: off (set OTA_PASSWORD_MD5 and OTA_SIGNING_KEY in config.h)."); }

#endif
