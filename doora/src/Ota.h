// Firmware updates over Wi-Fi from PlatformIO (`pio run -e ota -t upload`, see README.md).
//
// Two locks: the espota password (only its MD5 is in the firmware), and the image signature, which
// is what really matters, because espota sends the image over a second, unauthenticated TCP
// connection that anyone on the network could hijack. After the upload the door reads the new image
// back from flash and checks its RSA signature (OtaImage.h) against the public key built into the
// firmware. Until that check passes the old firmware stays the boot partition; a bad image is
// discarded and the door keeps running. Filesystem uploads are refused: they could not be signed,
// and replacing the flash storage would wipe the blacklist.
//
// Built only when config.h defines OTA_PASSWORD_MD5 and OTA_SIGNING_KEY.
#pragma once

namespace Ota {
// Starts the update listener in a task of its own, once Wi-Fi is up. `hostname` is also the mDNS
// name (hostname.local). Logs and does nothing when OTA is not configured.
void begin(const char* hostname);
}
