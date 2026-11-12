// Reassembles the chunked writes a phone sends (docs/ble-protocol.md, section 3). Each write is
// `header | data`; bit 7 of the header marks the last chunk and bits 0-6 are the chunk index from
// 0. Used by the bridge; portable so its limits are unit tested on a workstation.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

class ChunkAssembler {
 public:
	static constexpr size_t kMaxBody = 1024;
	enum class Status : uint8_t { More, Complete, Reject };

	// Reject: an out-of-order index, a write after the last chunk, an empty write or a body over
	// 1024 bytes. The caller disconnects; reset() starts over for the next connection.
	Status write(const uint8_t* data, size_t len) {
		if (done_ || len < 1) return Status::Reject;
		uint8_t header = data[0];
		if ((header & 0x7F) != next_ || next_ >= 0x7F + 1) return Status::Reject;
		if (n_ + len - 1 > kMaxBody) return Status::Reject;
		memcpy(buf_ + n_, data + 1, len - 1);
		n_ += len - 1;
		next_++;
		if (header & 0x80) {
			done_ = true;
			return Status::Complete;
		}
		return Status::More;
	}
	void reset() { n_ = 0; next_ = 0; done_ = false; }
	const uint8_t* body() const { return buf_; }
	size_t size() const { return n_; }

 private:
	uint8_t buf_[kMaxBody];
	size_t n_ = 0;
	uint8_t next_ = 0;
	bool done_ = false;
};
