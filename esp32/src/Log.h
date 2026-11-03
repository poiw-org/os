#pragma once
#include <Arduino.h>

// printf-style logging.
#define LOG(fmt, ...) Serial.printf(fmt "\n", ##__VA_ARGS__)
