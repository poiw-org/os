#pragma once
#include <Arduino.h>
#include <Term.h>  // angle brackets so the host tests can supply their own (host_test/stubs)

// printf-style logging, to USB and to an approved Bluetooth link.
#define LOG(fmt, ...) Term.printf(fmt "\n", ##__VA_ARGS__)
