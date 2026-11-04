// Finds the key token in the NDEF data of a tag. NFC Forum Type 2 tags (NTAG, Ultralight) and
// MIFARE Classic both store it as TLV blocks (03 = NDEF message) in one contiguous data area.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Must make the first n bytes of the data area valid, reading from the tag only what is missing,
// and return false when it cannot (read failure, or n beyond the buffer).
using NdefFill = bool (*)(void* ctx, size_t n);

// Walks the TLVs in `area` (the buffer `fill` writes into) up to the first NDEF message and
// returns the JWT in its first record as a view into `area`. Reads no more of the tag than needed.
bool ndefFindToken(const uint8_t* area, NdefFill fill, void* ctx, const uint8_t*& token, size_t& len);

// Encodes `token` the way the backoffice writes it with Web NFC: one text record (language "en")
// inside an NDEF TLV, then the terminator. Returns the byte count (not padded), or 0 when `cap` is
// too small. The result reads back through ndefFindToken().
size_t ndefEncodeToken(const char* token, size_t len, uint8_t* out, size_t cap);
