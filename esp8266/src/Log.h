#pragma once
#include <Arduino.h>

// printf-style logging with the format string kept in flash.
#define LOG(fmt, ...) Serial.printf_P(PSTR(fmt "\n"), ##__VA_ARGS__)
