// Over-the-air updates. The door downloads an admin-approved image from Supabase (Cloud.cpp) over
// the connection its REST task already uses, so there is no listener on the door.
//
// Images are not signed. What the door installs is decided by the approval it reads over TLS from
// Supabase, and the download must match the SHA-256 that approval names.
//
// A new image then boots on trial: the boot loader falls back to the old one if the door resets
// before confirm() is called, which happens once the door is back on the cloud.
#pragma once
#include <stddef.h>

namespace Ota {
void begin();
// Ends a trial that has run too long, and restarts into a new image when the door is idle.
void service(bool idle);

// The download, from the REST task. `error()` says why a step returned false.
bool open(size_t total);
bool write(const char* data, size_t len);
bool finish(const char* expectedSha256, const char* releaseId);  // on success the door restarts when idle
void abort();
const char* error();

// The release this boot is a trial of, or null.
const char* trialRelease();
void confirm();
// The release whose trial failed and has not been reported yet, or null.
const char* rolledBackRelease();
void clearRolledBack();
}  // namespace Ota
