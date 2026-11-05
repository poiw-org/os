// Signed firmware images for over-the-air updates. The build appends a trailer to firmware.bin:
//
//   image | RSASSA-PKCS1-v1_5 SHA-256 signature of image (256 bytes) | "DOORASIG"
//
// The ESP32 boot loader ignores bytes after the app image, so the same file also flashes over USB.
// Portable, so the check is unit-tested on the workstation.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Rsa.h"

constexpr char kOtaMagic[8] = {'D', 'O', 'O', 'R', 'A', 'S', 'I', 'G'};
constexpr size_t kOtaTrailer = kRsaBytes + sizeof(kOtaMagic);

// Reads `len` bytes at `offset` of the received image into `out`.
using OtaRead = bool (*)(void* ctx, size_t offset, uint8_t* out, size_t len);

// True when the `total` received bytes end in a trailer whose signature over the bytes before it
// verifies with `key`.
bool otaImageSigned(const RsaPublicKey& key, OtaRead read, void* ctx, size_t total);
