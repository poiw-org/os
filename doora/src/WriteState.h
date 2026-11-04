// Where a "write a key to a tag" session stands. Shared by the reader (which moves it along), the
// realtime channel (which reports it to the backoffice) and main.
#pragma once
#include <stdint.h>

// How long the door waits for the tag once armed.
constexpr uint32_t kWriteWindowMs = 60000;

enum class WriteState : uint8_t {
	Idle,
	Armed,       // waiting for the tag the key is bound to
	WrongTag,    // a different tag was presented; still waiting
	Writing,     // the right tag is on the reader
	Done,        // written and read back
	TooSmall,    // the tag cannot hold the key
	Unsupported, // not an NFC Forum Type 2 tag
	Failed,      // the tag moved away or refused
	Timeout,
	Cancelled,
	Rejected     // the door would not write this token (bad signature)
};

inline bool writeFinished(WriteState s) { return s >= WriteState::Done; }
inline bool writeActive(WriteState s) { return s == WriteState::Armed || s == WriteState::WrongTag || s == WriteState::Writing; }

// The names the backoffice receives.
inline const char* writeStateName(WriteState s) {
	switch (s) {
		case WriteState::Armed: return "ready";
		case WriteState::WrongTag: return "wrong_tag";
		case WriteState::Writing: return "writing";
		case WriteState::Done: return "done";
		case WriteState::TooSmall: return "too_small";
		case WriteState::Unsupported: return "unsupported";
		case WriteState::Failed: return "failed";
		case WriteState::Timeout: return "timeout";
		case WriteState::Cancelled: return "cancelled";
		case WriteState::Rejected: return "rejected";
		default: return "idle";
	}
}
