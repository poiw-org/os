// Stand-in for the Arduino core when Pn532.cpp is built on the workstation. Time is virtual:
// delays advance it instead of sleeping, so timeouts are deterministic and tests run instantly.
#pragma once
#include <stdint.h>

extern uint64_t g_nowUs;
inline uint32_t micros() { return (uint32_t)g_nowUs; }
inline uint32_t millis() { return (uint32_t)(g_nowUs / 1000); }
inline void delayMicroseconds(uint32_t us) { g_nowUs += us; }
inline void delay(uint32_t ms) { g_nowUs += (uint64_t)ms * 1000; }
inline void vTaskDelay(uint32_t ticks) { g_nowUs += (uint64_t)ticks * 1000; }

// Just enough of FreeRTOS and Serial for Reader.cpp (tasks are never started in the tests).
typedef void* TaskHandle_t;
typedef unsigned UBaseType_t;
#define pdMS_TO_TICKS(ms) (ms)
inline int xTaskCreatePinnedToCore(void (*)(void*), const char*, uint32_t, void*, UBaseType_t, TaskHandle_t*, int) { return 1; }
inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 0; }
struct HostSerial {
	int printf(const char*, ...) __attribute__((format(printf, 2, 3))) { return 0; }
};
extern HostSerial Serial;
