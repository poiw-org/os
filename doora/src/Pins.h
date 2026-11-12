// Wiring comes from the git-ignored config.h (copy config.example.h). The build stops if a pin is
// missing: guessing a relay pin could fire the door lock.
#pragma once
#include <Arduino.h>

#if __has_include("../config.h")
#include "../config.h"
#endif

#ifndef DOOR_PIN
#error "DOOR_PIN is not defined. Copy config.example.h to config.h and set your wiring."
#endif
#ifndef SPEAKER
#error "SPEAKER is not defined. Copy config.example.h to config.h and set your wiring."
#endif
// PN532 I2C bus. The default is the ESP32's usual I2C pair.
#ifndef NFC_SDA
#define NFC_SDA 21
#endif
#ifndef NFC_SCL
#define NFC_SCL 22
#endif
#ifndef BUSY_LED
#define BUSY_LED 32
#endif
#ifndef ERROR_LED
#define ERROR_LED 33
#endif
#ifndef SUCCESS_LED
#define SUCCESS_LED 23
#endif

// UART to the BLE bridge (an ESP32-C3, see src/bridge/). Free GPIOs on a WROOM module, and not
// strapping pins. BRIDGE_RESET is optional: when wired, the door pulses it (open-drain, low) if the
// bridge stays silent.
#ifndef BRIDGE_RX
#define BRIDGE_RX 16  // from the bridge's TX (GPIO 21)
#endif
#ifndef BRIDGE_TX
#define BRIDGE_TX 17  // to the bridge's RX (GPIO 20)
#endif
// #define BRIDGE_RESET 27
