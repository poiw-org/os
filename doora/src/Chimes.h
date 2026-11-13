// The door's sounds as pure functions of time: the pitch (Hz, 0 = silence) that should be playing
// `t` ms after the sound started. Actuators turns the answer into a tone on the speaker pin.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace Chimes {

// The HW-508 is a small passive magnetic buzzer resonant around 2.3-2.7 kHz: below ~1.9 kHz it is
// barely audible and above ~3 kHz it rolls off and buzzes harshly. Every pitch here stays within
// B6..F#7 (1976-2960 Hz), with the long, held notes nearest the resonance.

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

// The door's signature: B6, a leap up a fifth to F#7, then settling on D#7 (do-sol-mi), short-short-
// long. It opens upward like the door and lands on the bright major third, unresolved enough to
// beg the next round. One phrase per second, repeating for as long as the strike buzzes.
constexpr uint32_t kUnlockPhraseMs = 1000;
inline uint16_t unlocked(uint32_t t) {
	static constexpr Note kOpen[] = {{1976, 150}, {2960, 150}, {2489, 600}, {0, 100}};
	return at(kOpen, sizeof(kOpen) / sizeof(kOpen[0]), t, true);
}

// "Nuh-uh, no": two clipped F7 taps, then a drop of a tritone to B6 held, the interval nothing
// happy uses. Played once.
constexpr uint32_t kDeniedMs = 960;
inline uint16_t denied(uint32_t t) {
	static constexpr Note kSad[] = {{2794, 120}, {0, 60}, {2794, 120}, {0, 60}, {1976, 600}};
	return at(kSad, sizeof(kSad) / sizeof(kSad[0]), t, false);
}

// Two alternating tones with a pause, repeating, for the countdown before a force open.
inline uint16_t attention(uint32_t t) {
	static constexpr Note kAlert[] = {{2217, 200}, {2960, 200}, {0, 200}};
	return at(kAlert, sizeof(kAlert) / sizeof(kAlert[0]), t, true);
}

// The same two tones, four times as fast and without the pause, while a tag is being written.
inline uint16_t writing(uint32_t t) {
	static constexpr Note kFast[] = {{2217, 50}, {2960, 50}};
	return at(kFast, sizeof(kFast) / sizeof(kFast[0]), t, true);
}

}  // namespace Chimes
