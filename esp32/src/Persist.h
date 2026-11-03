// Flash storage on LittleFS for state that must survive a power cut: the blacklist, a clock
// checkpoint and the queue of door events not yet uploaded. Every file is replaced atomically
// (write a temp file, then rename) or is append-only with self-checking records, so a power loss
// at any instant leaves either the old or the new state, never a half-written one.
//
// Thread safe: the main loop appends events and saves the clock while the REST task uploads the
// journal and saves the blacklist. One lock covers every operation, and nothing here allocates.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "BlacklistSet.h"
#include "Journal.h"
#include "Rsa.h"

namespace Persist {
bool begin();

// False when the file is missing or fails its checksum; `out` is then left empty.
bool loadBlacklist(BlacklistSet& out);
bool saveBlacklist(const BlacklistSet& set);

// Public key that verifies v2 key tokens, as last announced by the backoffice.
bool loadSigningKey(uint8_t n[kRsaBytes]);
bool saveSigningKey(const uint8_t n[kRsaBytes]);

// Last time the door knew for sure (0 if never saved).
uint32_t loadClock();
bool saveClock(uint32_t epoch);

// Journal of door events. Capped; the oldest records are dropped first and counted.
constexpr size_t kJournalCapacity = 256;
bool journalAppend(JournalRecord record);
size_t journalCount();
uint32_t journalDropped();
// Copies up to `max` oldest records (valid or not; check journalValid). `seq` identifies the
// journal's front at the time of the read; hand it back to journalDrop().
size_t journalRead(JournalRecord* out, size_t max, uint32_t* seq);
// Removes the `count` records that journalRead() returned. If the journal was full in the
// meantime and shed some of them, only the remainder is removed, so a record that was never
// read is never dropped.
bool journalDrop(size_t count, uint32_t seq);
}  // namespace Persist
