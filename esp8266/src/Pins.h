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
#ifndef IDLE_LED
#define IDLE_LED D5
#endif
#ifndef BUSY_LED
#define BUSY_LED D6
#endif
#ifndef ERROR_LED
#define ERROR_LED D7
#endif
#ifndef SUCCESS_LED
#define SUCCESS_LED D8
#endif
