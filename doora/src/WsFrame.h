// RFC 6455 framing without any I/O, so it can be unit tested: a builder for the client frames the
// realtime protocol needs and an incremental parser for what the server sends.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace ws {

constexpr uint8_t kText = 0x1, kClose = 0x8, kPing = 0x9, kPong = 0xA;
// Largest client frame header: 2 bytes, a 16-bit length and the 4-byte mask.
constexpr size_t kMaxHeader = 8;

// Writes the header of a masked, final frame carrying `len` bytes (at most 65535) and returns its
// size, or 0 if `len` is too large.
size_t buildHeader(uint8_t* out, uint8_t opcode, size_t len, const uint8_t key[4]);
// Masks (or unmasks) a payload in place.
void mask(uint8_t* data, size_t len, const uint8_t key[4]);

// Turns a byte stream from the server into messages. Fragmented messages are reassembled, control
// frames may interleave with them, and anything the protocol forbids ends in Event::Error.
class Parser {
 public:
	enum class Event : uint8_t { None, Message, Dropped, Ping, Pong, Close, Error };

	// Message payloads are collected in `buf` (NUL terminated), which holds `cap` bytes: messages
	// of up to cap - 1 bytes are delivered, larger ones are skipped and reported as Dropped.
	void reset(uint8_t* buf, size_t cap);
	// Consumes input up to the end of the next event and reports how much it took in `*used`.
	// None means all input was consumed without finishing an event.
	Event feed(const uint8_t* data, size_t n, size_t* used);

	// Message: the payload is in the buffer given to reset() until the next message begins.
	size_t length() const { return msgLen_; }
	// Ping, Pong, Close: the frame's payload.
	const uint8_t* control() const { return ctl_; }
	size_t controlLength() const { return ctlLen_; }

 private:
	Event beginFrame();
	Event endFrame();
	void store(const uint8_t* p, size_t n);

	uint8_t* buf_ = nullptr;
	size_t cap_ = 0;
	size_t msgLen_ = 0;
	bool inMessage_ = false, overflow_ = false, failed_ = false;

	uint8_t head_[10];
	uint8_t have_ = 0, headNeed_ = 2;
	bool payload_ = false;
	bool control_ = false, fin_ = false;
	uint8_t opcode_ = 0;
	uint64_t remaining_ = 0;
	uint8_t ctl_[125];
	size_t ctlLen_ = 0;
};

}  // namespace ws
