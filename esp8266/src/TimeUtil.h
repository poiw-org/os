// Pure helpers for timestamps, tag UIDs and JSON text. No Arduino dependencies.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Seconds since 1970-01-01 UTC for a civil date; 0 if the date is outside 1970..2106.
uint32_t epochFromCivil(int year, int month, int day, int hour, int minute, int second);

// "2026-11-03T15:04:05Z"; `out` needs 21 bytes.
void formatIso(uint32_t epoch, char* out);

// Parses Postgres/ISO timestamps ("2026-11-03T15:04:05.123+00:00", "... Z", space separator).
// Returns 0 when the text is not a timestamp.
uint32_t parseIso(const char* text);

// A conservative "no earlier than" time derived from the compile timestamp (__DATE__, __TIME__).
// The builder's timezone is unknown, so a day is subtracted.
uint32_t buildEpoch(const char* date, const char* time);

// Tag UID <-> "04:a1:b2:c3". parseUid accepts ':' ' ' '-' separators and "0x" prefixes and
// returns the byte count, or -1 for malformed input.
size_t formatUid(const uint8_t* uid, size_t len, char* out, size_t cap);
int parseUid(const char* text, uint8_t* out, size_t cap);

// Writes `in` JSON-escaped (without quotes). Returns the length, or 0 if `cap` is too small.
size_t jsonEscape(const char* in, char* out, size_t cap);
