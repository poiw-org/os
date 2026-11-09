// The console over Bluetooth Classic (SPP). Links are paired and encrypted, and carry nothing until
// an admin has approved the device from the webapp (BtGate.h). While a link is open it mirrors the
// USB console: every log line goes out and typed commands come in. All of this is non-blocking, so
// a stalled phone cannot hold up the door; output that does not fit is dropped.
#pragma once
#include <stddef.h>
#include <stdint.h>

class Cloud;

namespace Bt {
// Off: Bluetooth Classic takes 79 KB of heap, and a WROOM module has about 100 KB in all, so TLS
// cannot run beside it. Needs a module with PSRAM, or a move to BLE.
constexpr bool kEnabled = false;
void begin(const char* name);
// Applies connections, approvals and deadlines. Call every loop and while a console prompt waits.
void service(Cloud& cloud);
int available();
int read();                               // -1 when nothing is waiting
void write(const uint8_t* data, size_t len);
void printStatus();
}  // namespace Bt
