// Restarts the chip. With RESET_PIN wired to this board's own EN pad, pulling it low resets the
// whole chip (peripherals and RTC domain included), which ESP.restart() does not. If the pad is not
// wired, or the chip is somehow still running after the pulse, it falls back to ESP.restart().
#pragma once
#include <Arduino.h>
#include "Pins.h"

[[noreturn]] inline void hardRestart() {
#ifdef RESET_PIN
	digitalWrite(RESET_PIN, LOW);
	pinMode(RESET_PIN, OUTPUT_OPEN_DRAIN);  // low only; EN's pull-up does the rest once the chip is gone
	delay(50);
#endif
	ESP.restart();
	for (;;) {}
}
