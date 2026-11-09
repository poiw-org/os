// The console's output for the portal. While an admin's session holds the lease, everything printed
// is queued here and the realtime task sends it to the private door channel. Nothing is kept or sent
// without a lease, and output that does not fit is dropped, so a slow network cannot hold up the door.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace RemoteConsole {
void begin();
void renewLease();                    // an open session repeats this; it lapses on its own
bool active();
void write(const uint8_t* data, size_t len);
size_t read(char* out, size_t max);   // from the realtime task
void mute(bool on);                   // the setup wizard echoes secrets as they are typed
}  // namespace RemoteConsole
