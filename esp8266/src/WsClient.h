// Minimal RFC 6455 websocket client on top of an already configured TLS client. Only what the
// Supabase realtime protocol needs: text messages, ping/pong and close. Non-blocking after open().
#pragma once
#include <WiFiClientSecureBearSSL.h>

class WsClient {
 public:
	explicit WsClient(BearSSL::WiFiClientSecure& tls) : tls_(tls) {}
	~WsClient() { close(); }

	// Connects, upgrades and allocates the receive buffer.
	bool open(const char* host, const char* path, uint32_t timeoutMs, size_t maxMessage);
	void close();
	bool connected() { return open_ && tls_.connected(); }

	enum class Event : uint8_t { None, Message, Dropped, Closed };
	// Reads whatever has arrived. Message: text()/length() are valid until the next poll().
	// Dropped: a message was larger than the buffer and was skipped.
	Event poll();
	const char* text() const { return (const char*)rx_; }
	size_t length() const { return rxLen_; }

	// Sends a text message. The buffer is masked in place, so it must be writable and is
	// unusable afterwards.
	bool sendText(char* data, size_t len);

 private:
	bool sendFrame(uint8_t opcode, uint8_t* data, size_t len);
	bool readHeaderLine(char* out, size_t cap, uint32_t deadline);
	BearSSL::WiFiClientSecure& tls_;
	bool open_ = false;

	uint8_t* rx_ = nullptr;
	size_t cap_ = 0, rxLen_ = 0;
	bool messageReady_ = false;

	// Frame parser state, kept across poll() calls.
	uint8_t hdr_[14];
	uint8_t hdrHave_ = 0, hdrNeed_ = 2;
	bool inPayload_ = false, fin_ = false, overflow_ = false;
	uint8_t opcode_ = 0;
	uint32_t remaining_ = 0;
	uint8_t ctl_[125];
	uint8_t ctlLen_ = 0;
};
