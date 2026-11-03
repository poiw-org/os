#include "WsFrame.h"
#include <string.h>

namespace ws {
namespace {
// A server has no reason to send more than this; a larger length is a broken or hostile peer.
constexpr uint64_t kMaxFrame = 16ull * 1024 * 1024;
}  // namespace

size_t buildHeader(uint8_t* out, uint8_t opcode, size_t len, const uint8_t key[4]) {
	if (len > 0xFFFF) return 0;
	size_t h = 0;
	out[h++] = 0x80 | opcode;
	if (len < 126) {
		out[h++] = 0x80 | (uint8_t)len;
	} else {
		out[h++] = 0x80 | 126;
		out[h++] = (uint8_t)(len >> 8);
		out[h++] = (uint8_t)len;
	}
	memcpy(out + h, key, 4);
	return h + 4;
}

void mask(uint8_t* data, size_t len, const uint8_t key[4]) {
	for (size_t i = 0; i < len; i++) data[i] ^= key[i & 3];
}

void Parser::reset(uint8_t* buf, size_t cap) {
	*this = Parser();
	buf_ = buf;
	cap_ = cap;
}

// The two header bytes (and the length extension) are complete.
Parser::Event Parser::beginFrame() {
	fin_ = head_[0] & 0x80;
	opcode_ = head_[0] & 0x0F;
	uint8_t len7 = head_[1] & 0x7F;
	uint64_t len = len7;
	if (len7 == 126) {
		len = (uint64_t)head_[2] << 8 | head_[3];
	} else if (len7 == 127) {
		len = 0;
		for (int i = 0; i < 8; i++) len = len << 8 | head_[2 + i];
	}
	have_ = 0;
	headNeed_ = 2;
	if (len > kMaxFrame) return Event::Error;

	control_ = opcode_ >= 0x8;
	if (control_) {
		if (!fin_ || len > sizeof(ctl_) || opcode_ > kPong) return Event::Error;
		ctlLen_ = 0;
	} else if (opcode_ == 0x0) {  // continuation
		if (!inMessage_) return Event::Error;
	} else if (opcode_ == 0x1 || opcode_ == 0x2) {
		if (inMessage_) return Event::Error;  // the previous message was never finished
		inMessage_ = true;
		msgLen_ = 0;
		overflow_ = false;
	} else {
		return Event::Error;
	}
	remaining_ = len;
	if (len == 0) return endFrame();
	payload_ = true;
	return Event::None;
}

Parser::Event Parser::endFrame() {
	payload_ = false;
	if (control_) {
		return opcode_ == kClose ? Event::Close : opcode_ == kPing ? Event::Ping : Event::Pong;
	}
	if (!fin_) return Event::None;
	inMessage_ = false;
	if (overflow_) {
		overflow_ = false;
		msgLen_ = 0;
		return Event::Dropped;
	}
	buf_[msgLen_] = 0;
	return Event::Message;
}

void Parser::store(const uint8_t* p, size_t n) {
	if (control_) {
		memcpy(ctl_ + ctlLen_, p, n);
		ctlLen_ += n;
		return;
	}
	if (overflow_) return;
	if (msgLen_ + n > cap_ - 1) {
		overflow_ = true;
		return;
	}
	memcpy(buf_ + msgLen_, p, n);
	msgLen_ += n;
}

Parser::Event Parser::feed(const uint8_t* data, size_t n, size_t* used) {
	size_t i = 0;
	Event result = Event::None;
	if (failed_) result = Event::Error;
	while (i < n && result == Event::None) {
		if (payload_) {
			size_t take = n - i;
			if (take > remaining_) take = (size_t)remaining_;
			store(data + i, take);
			i += take;
			remaining_ -= take;
			if (!remaining_) result = endFrame();
			continue;
		}
		head_[have_++] = data[i++];
		if (have_ == 2) {
			// No extension is negotiated, and a server never masks its frames (RFC 6455, 5.1).
			if ((head_[0] & 0x70) || (head_[1] & 0x80)) {
				result = Event::Error;
				break;
			}
			uint8_t len7 = head_[1] & 0x7F;
			headNeed_ = len7 == 126 ? 4 : len7 == 127 ? 10 : 2;
		}
		if (have_ == headNeed_) result = beginFrame();
	}
	if (result == Event::Error) failed_ = true;
	*used = i;
	return result;
}

}  // namespace ws
