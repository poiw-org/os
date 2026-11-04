// The door's sounds as pure functions of time: the pitch (Hz, 0 = silence) that should be playing
// `t` ms after the sound started. Actuators turns the answer into a tone on the speaker pin.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace Chimes {

struct Note {
	uint16_t hz;
	uint16_t ms;
};

// Time to wait between a force-open command and the relay, so anyone at the door is warned.
constexpr uint32_t kForceOpenDelayMs = 3000;

inline uint16_t at(const Note* notes, size_t count, uint32_t t, bool loop) {
	uint32_t total = 0;
	for (size_t i = 0; i < count; i++) total += notes[i].ms;
	if (loop) t %= total;
	for (size_t i = 0; i < count; i++) {
		if (t < notes[i].ms) return notes[i].hz;
		t -= notes[i].ms;
	}
	return 0;
}

// The attention tones at double speed, repeating while the door is unlocked.
inline uint16_t unlocked(uint32_t t) {
	static constexpr Note kBuzz[] = {{2640, 100}, {3525, 100}, {0, 100}};
	return at(kBuzz, sizeof(kBuzz) / sizeof(kBuzz[0]), t, true);
}

// A minor chord (E C A) falling once, up where a piezo is loudest.
constexpr uint32_t kDeniedMs = 1150;
inline uint16_t denied(uint32_t t) {
	static constexpr Note kSad[] = {{2636, 150}, {2092, 150}, {1760, 150}, {880, 700}};
	return at(kSad, sizeof(kSad) / sizeof(kSad[0]), t, false);
}

// Two alternating tones with a pause, repeating, for the countdown before a force open.
inline uint16_t attention(uint32_t t) {
	static constexpr Note kAlert[] = {{2640, 200}, {3525, 200}, {0, 200}};
	return at(kAlert, sizeof(kAlert) / sizeof(kAlert[0]), t, true);
}

// The same two tones, four times as fast and without the pause, while a tag is being written.
inline uint16_t writing(uint32_t t) {
	static constexpr Note kFast[] = {{2640, 50}, {3525, 50}};
	return at(kFast, sizeof(kFast) / sizeof(kFast[0]), t, true);
}

}  // namespace Chimes
