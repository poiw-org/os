// PN532 host frames (UM0701-02 section 6.2.1): 00 00 FF LEN LCS TFI data... DCS 00.
// Pure functions, so the framing is unit-tested on the workstation.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace pn532 {

// The chip answers every command frame with this, then (later) the response frame. The host also
// sends it to abort a command that is still running.
inline constexpr uint8_t kAckFrame[6] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};

constexpr uint8_t kTfiToChip = 0xD4;
constexpr uint8_t kTfiFromChip = 0xD5;

// Writes the frame for a command (payload[0] is the command code) and returns its size, n + 8.
// `out` must have room for that.
size_t buildFrame(uint8_t* out, const uint8_t* payload, size_t n);

// True for the 5 bytes 00 00 FF LEN LCS with a consistent length checksum.
bool headerValid(const uint8_t h[5]);

}  // namespace pn532
