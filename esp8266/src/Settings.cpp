#include "Settings.h"
#include <EEPROM.h>
#include <string.h>
#include "Crc.h"

namespace {
constexpr uint32_t kMagic = 0x31534444;  // "DDS1"
constexpr uint16_t kVersion = 1;

struct Header {
	uint32_t magic;
	uint16_t version;
	uint16_t size;
	uint32_t crc;  // over the Settings bytes
};
constexpr size_t kTotal = sizeof(Header) + sizeof(Settings);
}  // namespace

namespace SettingsStore {

bool complete(const Settings& s) {
	return s.wifiSsid[0] && s.supabaseHost[0] && s.anonKey[0] && s.email[0] && s.password[0] && s.doorId[0] &&
	       s.jwtSecret[0] && s.ntpServer[0];
}

bool load(Settings& out) {
	EEPROM.begin(kTotal);
	Header h;
	EEPROM.get(0, h);
	Settings s;
	EEPROM.get(sizeof(Header), s);
	EEPROM.end();

	if (h.magic != kMagic || h.version != kVersion || h.size != sizeof(Settings)) return false;
	if (h.crc != crc32((const uint8_t*)&s, sizeof(s))) return false;
	// Fields are used as C strings; make sure a damaged-but-checksummed image cannot overrun them.
	s.wifiSsid[sizeof(s.wifiSsid) - 1] = 0;
	s.wifiPass[sizeof(s.wifiPass) - 1] = 0;
	s.supabaseHost[sizeof(s.supabaseHost) - 1] = 0;
	s.anonKey[sizeof(s.anonKey) - 1] = 0;
	s.email[sizeof(s.email) - 1] = 0;
	s.password[sizeof(s.password) - 1] = 0;
	s.doorId[sizeof(s.doorId) - 1] = 0;
	s.jwtSecret[sizeof(s.jwtSecret) - 1] = 0;
	s.ntpServer[sizeof(s.ntpServer) - 1] = 0;
	if (!complete(s)) return false;
	out = s;
	return true;
}

bool save(const Settings& s) {
	Header h = {kMagic, kVersion, (uint16_t)sizeof(Settings), crc32((const uint8_t*)&s, sizeof(s))};
	EEPROM.begin(kTotal);
	EEPROM.put(0, h);
	EEPROM.put(sizeof(Header), s);
	bool committed = EEPROM.commit();
	EEPROM.end();
	// Read it back: a failed or partial commit must not be reported as saved.
	Settings check;
	return committed && load(check) && memcmp(&check, &s, sizeof(s)) == 0;
}

void erase() {
	EEPROM.begin(kTotal);
	for (size_t i = 0; i < kTotal; i++) EEPROM.write(i, 0xFF);
	EEPROM.commit();
	EEPROM.end();
}

}  // namespace SettingsStore
