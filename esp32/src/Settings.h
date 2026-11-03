// Everything the door needs to know that is not in the firmware: Wi-Fi, Supabase and the key
// signing secret. Stored in EEPROM (flash-emulated) with a checksum, and entered over serial.
#pragma once
#include <stddef.h>
#include <stdint.h>

struct Settings {
	char wifiSsid[33];
	char wifiPass[65];
	char supabaseHost[64];  // <project>.supabase.co, no scheme
	char anonKey[256];      // the project's publishable (anon) key
	char email[64];         // the door's Supabase user
	char password[64];
	char doorId[32];        // row id in public.doors; also the realtime channel name
	char jwtSecret[129];     // must equal the edge function's JWT_SIGNING_KEY
	char ntpServer[40];
};

enum class Category : uint8_t { Wifi, Supabase, Door, Time };

namespace SettingsStore {
// Like load() but accepts an incomplete image (after a partial erase), so the wizard can prefill
// the fields that were kept.
bool loadRaw(Settings& out);
// Blanks one category's fields in `s`; the caller saves the result.
void clear(Settings& s, Category c);
// False when nothing valid is stored (blank flash, bad checksum, other version) or a required
// field is empty.
bool load(Settings& out);
bool save(const Settings& s);
void erase();
bool complete(const Settings& s);
}  // namespace SettingsStore
