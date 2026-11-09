// Firmware updates over the console, sent by tools/bt_update.py over an approved Bluetooth link.
//
// The image signature is the lock that matters: after the upload the door reads the new image back
// from flash and checks its RSA signature (OtaImage.h) against the public key built into the
// firmware. Until that check passes the old firmware stays the boot partition, so a bad image is
// simply never started.
//
// Built only when config.h defines OTA_SIGNING_KEY.
#pragma once
#include <stddef.h>

namespace Ota {
void begin();
// The `update <bytes>` command. Takes `total` bytes from the console in chunks, answering each with
// an ACK byte, and restarts into a valid image. `idle` keeps the door working meanwhile.
void receive(size_t total, void (*idle)());
}
