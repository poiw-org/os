#include "WsClient.h"
#include <Arduino.h>
#include <string.h>
#include "Log.h"

namespace {
const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

bool WsClient::readHeaderLine(char* out, size_t cap, uint32_t deadline) {
	size_t n = 0;
	for (;;) {
		if ((int32_t)(millis() - deadline) >= 0) return false;
		if (!tls_.available()) {
			if (!tls_.connected()) return false;
			delay(2);
			continue;
		}
		int c = tls_.read();
		if (c == '\n') { out[n] = 0; return true; }
		if (c != '\r' && n + 1 < cap) out[n++] = (char)c;
	}
}

bool WsClient::open(const char* host, const char* path, uint32_t timeoutMs, size_t maxMessage) {
	close();
	rx_ = (uint8_t*)malloc(maxMessage + 1);
	if (!rx_) return false;
	cap_ = maxMessage;

	if (!tls_.connect(host, 443)) {
		char err[80] = "";
		tls_.lastError(err, sizeof(err));
		LOG("WS: TLS connect failed (%s).", err);
		close();
		return false;
	}

	uint8_t raw[16];
	for (auto& b : raw) b = (uint8_t)esp_random();
	char key[25];
	for (int i = 0; i < 5; i++) {
		uint32_t v = (uint32_t)raw[i * 3] << 16 | (uint32_t)raw[i * 3 + 1] << 8 | raw[i * 3 + 2];
		for (int j = 0; j < 4; j++) key[i * 4 + j] = kB64[(v >> (18 - 6 * j)) & 63];
	}
	{
		uint32_t v = (uint32_t)raw[15] << 16;
		key[20] = kB64[(v >> 18) & 63]; key[21] = kB64[(v >> 12) & 63]; key[22] = '='; key[23] = '=';
	}
	key[24] = 0;

	tls_.print(F("GET "));
	tls_.print(path);
	tls_.print(F(" HTTP/1.1\r\nHost: "));
	tls_.print(host);
	tls_.print(F("\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: "));
	tls_.print(key);
	tls_.print(F("\r\n\r\n"));
	tls_.flush();

	uint32_t deadline = millis() + timeoutMs;
	char line[120];
	if (!readHeaderLine(line, sizeof(line), deadline) || !strstr(line, " 101")) {
		LOG("WS: upgrade refused: %s", line);
		close();
		return false;
	}
	while (readHeaderLine(line, sizeof(line), deadline)) {
		if (!line[0]) { open_ = true; break; }
	}
	if (!open_) { close(); return false; }

	hdrHave_ = 0; hdrNeed_ = 2; inPayload_ = false; overflow_ = false; rxLen_ = 0; messageReady_ = false;
	return true;
}

void WsClient::close() {
	if (open_ && tls_.connected()) {
		uint8_t code[2] = {0x03, 0xE8};  // 1000: normal closure
		sendFrame(0x8, code, 2);
	}
	tls_.stop();
	open_ = false;
	free(rx_);
	rx_ = nullptr;
	cap_ = rxLen_ = 0;
}

bool WsClient::sendFrame(uint8_t opcode, uint8_t* data, size_t len) {
	if (!tls_.connected()) return false;
	uint8_t head[8];
	size_t h = 0;
	head[h++] = 0x80 | opcode;
	if (len < 126) head[h++] = 0x80 | (uint8_t)len;
	else { head[h++] = 0x80 | 126; head[h++] = (uint8_t)(len >> 8); head[h++] = (uint8_t)len; }
	uint8_t mask[4];
	uint32_t r = esp_random();
	memcpy(mask, &r, 4);
	for (size_t i = 0; i < len; i++) data[i] ^= mask[i & 3];
	return tls_.write(head, h) == h && tls_.write(mask, 4) == 4 && (len == 0 || tls_.write(data, len) == len) &&
	       (tls_.flush(), true);
}

bool WsClient::sendText(char* data, size_t len) { return open_ && sendFrame(0x1, (uint8_t*)data, len); }

WsClient::Event WsClient::poll() {
	if (!open_) return Event::Closed;
	if (messageReady_) { messageReady_ = false; rxLen_ = 0; }

	while (tls_.available()) {
		if (!inPayload_) {
			int c = tls_.read();
			if (c < 0) break;
			hdr_[hdrHave_++] = (uint8_t)c;
			if (hdrHave_ == 2) {
				uint8_t len7 = hdr_[1] & 0x7F;
				hdrNeed_ = 2 + (len7 == 126 ? 2 : len7 == 127 ? 8 : 0) + ((hdr_[1] & 0x80) ? 4 : 0);
			}
			if (hdrHave_ < hdrNeed_) continue;

			fin_ = hdr_[0] & 0x80;
			uint8_t op = hdr_[0] & 0x0F;
			uint8_t len7 = hdr_[1] & 0x7F;
			uint64_t len = len7;
			if (len7 == 126) len = (uint64_t)hdr_[2] << 8 | hdr_[3];
			else if (len7 == 127) { len = 0; for (int i = 0; i < 8; i++) len = len << 8 | hdr_[2 + i]; }
			hdrHave_ = 0;
			if (len > 0x10000000ull || (op >= 8 && len > 125)) { close(); return Event::Closed; }
			if (op != 0) opcode_ = op;  // continuation frames keep the message's opcode
			remaining_ = (uint32_t)len;
			ctlLen_ = 0;
			inPayload_ = true;
		}

		bool control = opcode_ >= 8;
		while (remaining_ && tls_.available()) {
			if (control) {
				int c = tls_.read();
				if (c < 0) break;
				ctl_[ctlLen_++] = (uint8_t)c;
				remaining_--;
			} else {
				size_t want = remaining_;
				size_t have = (size_t)tls_.available();
				if (want > have) want = have;
				if (!overflow_ && rxLen_ + want > cap_) overflow_ = true;
				if (overflow_) {
					uint8_t sink[64];
					if (want > sizeof(sink)) want = sizeof(sink);
					int got = tls_.read(sink, want);
					if (got <= 0) break;
					remaining_ -= (uint32_t)got;
				} else {
					int got = tls_.read(rx_ + rxLen_, want);
					if (got <= 0) break;
					rxLen_ += (size_t)got;
					remaining_ -= (uint32_t)got;
				}
			}
		}
		if (remaining_) break;  // wait for more bytes
		inPayload_ = false;

		if (control) {
			if (opcode_ == 0x8) { close(); return Event::Closed; }
			if (opcode_ == 0x9) sendFrame(0xA, ctl_, ctlLen_);  // ping -> pong
			continue;
		}
		if (!fin_) continue;
		if (overflow_) {
			overflow_ = false;
			rxLen_ = 0;
			return Event::Dropped;
		}
		rx_[rxLen_] = 0;
		messageReady_ = true;
		return Event::Message;
	}
	if (!tls_.connected() && !tls_.available()) { close(); return Event::Closed; }
	return Event::None;
}
