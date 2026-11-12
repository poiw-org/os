// The UART link between the door and the BLE bridge (docs/ble-protocol.md, section 6): COBS framing,
// a CRC and the frame types. Portable and allocation-free. The door and the bridge firmware compile
// this same file, so the two ends cannot disagree about the wire format.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace Link {

enum Type : uint8_t {
	kHello = 0x10,      // bridge -> door
	kPing = 0x11,       // door -> bridge
	kPong = 0x12,       // bridge -> door
	kConnected = 0x20,  // bridge -> door
	kChallenge = 0x21,  // door -> bridge
	kRequest = 0x22,    // bridge -> door
	kResult = 0x23,     // door -> bridge
	kGone = 0x24        // bridge -> door
};

constexpr uint8_t kVersion = 0x01;
constexpr size_t kMaxPayload = 1100;
constexpr size_t kMaxDecoded = 1 + 1 + kMaxPayload + 2;                // type, conn, payload, crc
constexpr size_t kMaxEncoded = kMaxDecoded + kMaxDecoded / 254 + 2;    // COBS overhead and the 0x00

bool knownType(uint8_t type);

// Builds one frame (type | conn | payload | crc16, COBS-encoded, ending in 0x00) into `out`.
// Returns its length, or 0 when the payload is too long or `cap` too small.
size_t encode(uint8_t type, uint8_t conn, const uint8_t* payload, size_t len, uint8_t* out, size_t cap);

// A decoded frame. `payload` points into the decoder and is valid until the next feed().
struct Frame {
	uint8_t type;
	uint8_t conn;
	const uint8_t* payload;
	size_t len;
};

// Fed one byte at a time from the UART. Garbage between frames is skipped: a frame that is too
// long, fails its CRC, has an unknown type or an oversized payload is counted and dropped, and the
// decoder resynchronises at the next 0x00.
class Decoder {
 public:
	// True when `byte` completed a valid frame; read it with frame().
	bool feed(uint8_t byte);
	const Frame& frame() const { return frame_; }
	uint32_t dropped() const { return dropped_; }

 private:
	uint8_t buf_[kMaxEncoded];
	size_t n_ = 0;
	bool overflow_ = false;
	uint32_t dropped_ = 0;
	Frame frame_ = {};
};

}  // namespace Link
