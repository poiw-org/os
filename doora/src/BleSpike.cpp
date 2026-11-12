// Measurement only (`make release ENV=ble_spike`): starts a BLE peripheral shaped like the planned
// unlock service (one service, one write+notify characteristic, one connection, no pairing) and
// records what it costs in heap. `status` then shows whether TLS still has the 72 KB it needs (Net.cpp).
//
// BLE starts only once the door is online and the update is confirmed, so a failure here cannot
// roll the firmware back. The attempt is written to flash first: if starting BLE resets the door,
// the next boot leaves BLE off and `status` gives the reset reason instead of trying again.
#ifdef BLE_SPIKE
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_system.h>
#include "Log.h"
#include "Ota.h"

namespace {
constexpr uint32_t kStartAfterMs = 60000;
constexpr const char* kPrefs = "blespike";
enum State : uint8_t { Fresh = 0, Trying = 1, Ran = 2, Reset = 3 };

char gName[48];
uint8_t gState = Fresh;
uint8_t gResetReason = 0;
bool gRunning = false;
uint32_t gBefore = 0, gAfter = 0, gBlock = 0;

// A new build gets a new attempt.
uint32_t buildId() {
	uint32_t h = 2166136261u;
	for (const char* p = __DATE__ __TIME__; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
	return h;
}

void store(uint8_t state) {
	Preferences p;
	p.begin(kPrefs, false);
	p.putUInt("build", buildId());
	p.putUChar("state", state);
	p.putUChar("reason", gResetReason);
	p.end();
	gState = state;
}

void start() {
	store(Trying);
	gBefore = ESP.getFreeHeap();
	NimBLEDevice::init(gName);  // hands the Bluetooth Classic half of the controller back to the heap
	NimBLEServer* server = NimBLEDevice::createServer();
	NimBLEService* service = server->createService("7d0a0001-6f2b-4c1e-9a53-d00a00000001");
	service->createCharacteristic("7d0a0002-6f2b-4c1e-9a53-d00a00000001",
	                              NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY, 244);
	service->start();
	NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
	adv->addServiceUUID(service->getUUID());
	adv->start();
	gAfter = ESP.getFreeHeap();
	gBlock = ESP.getMaxAllocHeap();
	gRunning = true;
	store(Ran);
	LOG("BLE spike: advertising as \"%s\"; free heap %u -> %u, largest block %u.", gName, gBefore, gAfter, gBlock);
}
}  // namespace

void bleSpikeBegin(const char* name) {
	strlcpy(gName, name, sizeof(gName));
	Preferences p;
	p.begin(kPrefs, true);
	bool sameBuild = p.getUInt("build", 0) == buildId();
	gState = sameBuild ? p.getUChar("state", Fresh) : Fresh;
	gResetReason = sameBuild ? p.getUChar("reason", 0) : 0;
	p.end();
	if (gState == Trying) {  // the last boot died while starting BLE
		gResetReason = (uint8_t)esp_reset_reason();
		store(Reset);
	}
	if (gState == Reset) LOG("BLE spike: starting BLE reset the door (reset reason %u); BLE stays off.", gResetReason);
}

void bleSpikeService(bool online) {
	if (gRunning || gState == Reset || !online || Ota::trialRelease() || millis() < kStartAfterMs) return;
	start();
}

void bleSpikePrintStatus() {
	if (gRunning)
		LOG("ble spike : advertising; free heap %u -> %u at start (cost %ld), largest block then %u", gBefore, gAfter,
		    (long)gBefore - (long)gAfter, gBlock);
	else if (gState == Reset)
		LOG("ble spike : off; starting BLE reset the door, esp_reset_reason %u", gResetReason);
	else
		LOG("ble spike : waiting for the door to be online and the update confirmed");
}
#endif
