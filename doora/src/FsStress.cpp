// TEMPORARY crash hunt (LittleFS fstat on a freed fd). Built only with -DDOORA_FS_STRESS.
// Replays the boot-time pattern: the cloud task saving the blacklist on core 0 while loop()
// saves the clock checkpoint on core 1, many times over, with Wi-Fi and TLS running.
#ifdef DOORA_FS_STRESS
#include <Arduino.h>
#include <esp_heap_caps.h>
#include "Log.h"
#include "Persist.h"

namespace {
constexpr int kRounds = 20000;
BlacklistSet gSet;

void blacklistWriter(void*) {
	vTaskDelay(pdMS_TO_TICKS(20000));  // after boot: Persist mounted, network up
	Persist::loadBlacklist(gSet);
	int fails = 0;
	for (int i = 1; i <= kRounds; i++) {
		if (!Persist::saveBlacklist(gSet)) fails++;
		if (i % 250 == 0) LOG("FSSTRESS blacklist %d/%d (fails %d, heap %u)", i, kRounds, fails, ESP.getFreeHeap());
	}
	LOG("FSSTRESS blacklist done");
	vTaskDelete(nullptr);
}

void clockWriter(void*) {
	vTaskDelay(pdMS_TO_TICKS(20000));
	uint32_t t = Persist::loadClock();
	int fails = 0;
	for (int i = 1; i <= kRounds; i++) {
		if (!Persist::saveClock(t)) fails++;
		if (i % 250 == 0) LOG("FSSTRESS clock %d/%d (fails %d)", i, kRounds, fails);
	}
	LOG("FSSTRESS clock done");
	vTaskDelete(nullptr);
}

// Checks every heap block's poisoning every second: corruption shows up near where it happened.
void heapWatch(void*) {
	for (uint32_t n = 0;; n++) {
		if (!heap_caps_check_integrity_all(true)) LOG("FSSTRESS HEAP CORRUPT (check %lu)", (unsigned long)n);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

struct Start {
	Start() {
		xTaskCreatePinnedToCore(heapWatch, "hw", 3072, nullptr, 1, nullptr, 0);
		xTaskCreatePinnedToCore(blacklistWriter, "fs0", 6144, nullptr, 1, nullptr, 0);
		xTaskCreatePinnedToCore(clockWriter, "fs1", 6144, nullptr, 1, nullptr, 1);
	}
} gStart;
}  // namespace
#endif
