// Minimal RFC 6455 websocket client over TLS, for the Supabase realtime protocol: text messages,
// ping/pong and close. The framing lives in WsFrame; this class connects, upgrades and moves
// bytes. All buffers are part of the object, so opening and closing a connection never touches
// the heap.
#pragma once
#include <WiFiClientSecure.h>
#include "WsFrame.h"

class WsClient {
 public:
	static constexpr size_t kMaxMessage = 3072;  // larger incoming messages are skipped
	static constexpr size_t kMaxSend = 1920;     // largest outgoing message

	enum class Event : uint8_t { None, Message, Dropped, Closed };

	~WsClient() { close(); }

	// Connects, upgrades and gets ready to exchange messages.
	bool open(const char* host, const char* path, uint32_t timeoutMs);
	void close();
	bool connected() { return open_ && tls_.connected(); }

	// Sleeps until something arrived or `ms` passed; returns true when poll() has work to do.
	bool wait(uint32_t ms);
	// Handles whatever has arrived, one event at a time. Message: text() and length() stay valid
	// until the next poll(). Dropped: a message was larger than kMaxMessage and was skipped.
	Event poll();
	const char* text() const { return reinterpret_cast<const char*>(rx_); }
	size_t length() const { return parser_.length(); }

	// Outgoing messages are composed directly in this buffer (kMaxSend bytes) and sent with one
	// write, so they leave as a single TLS record. The buffer is masked in place by send() and is
	// unusable afterwards.
	char* sendBuffer() { return reinterpret_cast<char*>(tx_ + ws::kMaxHeader); }
	bool send(size_t len);

 private:
	bool sendFrame(uint8_t opcode, uint8_t* payload, size_t len);
	bool sendControl(uint8_t opcode, const uint8_t* data, size_t len);
	bool readHeaderLine(char* out, size_t cap, uint32_t deadline);

	WiFiClientSecure tls_;
	ws::Parser parser_;
	bool open_ = false;
	uint8_t in_[256];  // bytes read from TLS that the parser has not consumed yet
	size_t inPos_ = 0, inLen_ = 0;
	uint8_t rx_[kMaxMessage + 1];
	uint8_t tx_[ws::kMaxHeader + kMaxSend];
};
