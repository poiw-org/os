#include "WsClient.h"
#include <Arduino.h>
#include <string.h>
#include "Log.h"
#include "Net.h"

namespace {
const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Sec-WebSocket-Key: 16 random bytes in base64.
void makeKey(char key[25]) {
	uint8_t raw[16];
	for (auto& b : raw) b = (uint8_t)esp_random();
	for (int i = 0; i < 5; i++) {
		uint32_t v = (uint32_t)raw[i * 3] << 16 | (uint32_t)raw[i * 3 + 1] << 8 | raw[i * 3 + 2];
		for (int j = 0; j < 4; j++) key[i * 4 + j] = kB64[(v >> (18 - 6 * j)) & 63];
	}
	uint32_t v = (uint32_t)raw[15] << 16;
	key[20] = kB64[(v >> 18) & 63];
	key[21] = kB64[(v >> 12) & 63];
	key[22] = key[23] = '=';
	key[24] = 0;
}
}  // namespace

bool WsClient::readHeaderLine(char* out, size_t cap, uint32_t deadline) {
	size_t n = 0;
	for (;;) {
		int32_t left = (int32_t)(deadline - millis());
		if (left <= 0) return false;
		if (tls_.available() <= 0) {
			if (!tls_.connected()) return false;
			Net::waitReadable(tls_, left < 100 ? (uint32_t)left : 100);
			continue;
		}
		int c = tls_.read();
		if (c == '\n') { out[n] = 0; return true; }
		if (c != '\r' && c >= 0 && n + 1 < cap) out[n++] = (char)c;
	}
}

bool WsClient::open(const char* host, const char* path, uint32_t timeoutMs) {
	close();
	if (!Net::connect(tls_, host, timeoutMs)) return false;

	char key[25];
	makeKey(key);
	// The whole upgrade request in one write.
	int n = snprintf(sendBuffer(), kMaxSend,
	                 "GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
	                 "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: %s\r\n\r\n",
	                 path, host, key);
	if (n <= 0 || (size_t)n >= kMaxSend || tls_.write(reinterpret_cast<const uint8_t*>(sendBuffer()), (size_t)n) != (size_t)n) {
		tls_.stop();
		return false;
	}

	uint32_t deadline = millis() + timeoutMs;
	char line[120] = "";
	if (!readHeaderLine(line, sizeof(line), deadline) || !strstr(line, " 101")) {
		LOG("WS: upgrade refused: %s", line);
		tls_.stop();
		return false;
	}
	bool complete = false;
	while (readHeaderLine(line, sizeof(line), deadline)) {
		if (!line[0]) { complete = true; break; }
	}
	if (!complete) {
		tls_.stop();
		return false;
	}
	parser_.reset(rx_, sizeof(rx_));
	inPos_ = inLen_ = 0;
	open_ = true;
	return true;
}

void WsClient::close() {
	if (open_ && tls_.connected()) {
		const uint8_t code[2] = {0x03, 0xE8};  // 1000: normal closure
		sendControl(ws::kClose, code, sizeof(code));
	}
	open_ = false;
	tls_.stop();
	inPos_ = inLen_ = 0;
}

bool WsClient::sendFrame(uint8_t opcode, uint8_t* payload, size_t len) {
	uint8_t key[4];
	uint32_t r = esp_random();
	memcpy(key, &r, sizeof(key));
	ws::mask(payload, len, key);
	uint8_t head[ws::kMaxHeader];
	size_t h = ws::buildHeader(head, opcode, len, key);
	if (!h) return false;
	uint8_t* frame = payload - h;  // the header goes right in front of the payload
	memcpy(frame, head, h);
	// A failed write stops the TLS client, which connected() and poll() then report.
	return tls_.write(frame, h + len) == h + len;
}

bool WsClient::send(size_t len) { return open_ && len <= kMaxSend && sendFrame(ws::kText, tx_ + ws::kMaxHeader, len); }

// Pong and close frames are small and may be sent while a message is being composed, so they
// use their own buffer.
bool WsClient::sendControl(uint8_t opcode, const uint8_t* data, size_t len) {
	uint8_t buf[ws::kMaxHeader + 125];
	if (len > 125) return false;
	memcpy(buf + ws::kMaxHeader, data, len);
	return sendFrame(opcode, buf + ws::kMaxHeader, len);
}

bool WsClient::wait(uint32_t ms) {
	if (!open_) return true;  // poll() reports the closed connection
	if (inPos_ < inLen_) return true;
	return Net::waitReadable(tls_, ms);
}

WsClient::Event WsClient::poll() {
	if (!open_) return Event::Closed;
	for (;;) {
		if (inPos_ == inLen_) {
			inPos_ = inLen_ = 0;
			int avail = tls_.available();
			if (avail <= 0) break;
			int got = tls_.read(in_, (size_t)avail < sizeof(in_) ? (size_t)avail : sizeof(in_));
			if (got <= 0) break;
			inLen_ = (size_t)got;
		}
		size_t used = 0;
		ws::Parser::Event e = parser_.feed(in_ + inPos_, inLen_ - inPos_, &used);
		inPos_ += used;
		switch (e) {
			case ws::Parser::Event::None: break;  // all input consumed: read more
			case ws::Parser::Event::Message: return Event::Message;
			case ws::Parser::Event::Dropped: return Event::Dropped;
			case ws::Parser::Event::Ping: sendControl(ws::kPong, parser_.control(), parser_.controlLength()); break;
			case ws::Parser::Event::Pong: break;
			case ws::Parser::Event::Close:
				close();
				return Event::Closed;
			case ws::Parser::Event::Error:
				LOG("WS: protocol error; closing.");
				close();
				return Event::Closed;
		}
	}
	if (!tls_.connected()) {
		close();
		return Event::Closed;
	}
	return Event::None;
}
