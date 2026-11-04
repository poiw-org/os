// TLS plumbing shared by the REST and the realtime task. Importers: Cloud.cpp, WsClient.cpp.
#pragma once
#include <WiFiClientSecure.h>
#include <stdint.h>

namespace Net {

// Opens a verified TLS connection to `host`:443. It refuses when free memory is too low for a
// session, lets only one handshake run at a time (the transient peak is the largest memory use of
// the whole firmware), and turns on TCP_NODELAY so small frames leave immediately instead of
// waiting for the previous one to be acknowledged. Logs why it failed.
bool connect(WiFiClientSecure& tls, const char* host, uint32_t timeoutMs);

// Sleeps until decrypted data is waiting on `tls` or `ms` have passed. Unlike polling available()
// it uses no CPU, and a message that arrives wakes the caller at once. False on timeout; a closed
// connection also returns, so check tls.connected() afterwards.
bool waitReadable(WiFiClientSecure& tls, uint32_t ms);

// Connection attempts refused for lack of memory since boot.
uint32_t lowMemoryRefusals();

}  // namespace Net
