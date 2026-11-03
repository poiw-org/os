#include "Console.h"
#include <Arduino.h>
#include <string.h>
#include "Log.h"

namespace {

constexpr uint32_t kWizardIdleMs = 180000;

// Reads one line from serial, echoing it. Returns its length, or -1 after `timeoutMs` without input
// (0 = wait forever). Keeps the watchdog fed while waiting. `idleSubmitMs` > 0 also accepts the text
// after that long a pause, for monitors set to "no line ending" (short answers only).
int readLine(char* out, size_t cap, uint32_t timeoutMs, bool echo, uint32_t idleSubmitMs = 0) {
	size_t n = 0;
	uint32_t last = millis();
	for (;;) {
		while (Serial.available()) {
			int c = Serial.read();
			last = millis();
			if (c == '\r' || c == '\n') {
				if (n == 0 && c == '\n') continue;  // the LF of a CRLF pair
				out[n] = 0;
				Serial.println();
				return (int)n;
			}
			if (c == 0x08 || c == 0x7f) {
				if (n) { n--; if (echo) Serial.print(F("\b \b")); }
				continue;
			}
			if (c < 0x20 || n + 1 >= cap) continue;
			out[n++] = (char)c;
			if (echo) Serial.write((uint8_t)c);
		}
		if (idleSubmitMs && n && millis() - last > idleSubmitMs) {
			out[n] = 0;
			Serial.println();
			return (int)n;
		}
		if (timeoutMs && millis() - last > timeoutMs) return -1;
		delay(5);
	}
}

typedef const char* (*Validator)(const char*);

const char* vSsid(const char* v) { return strlen(v) > 32 ? "at most 32 characters" : nullptr; }
const char* vWifiPass(const char* v) { return strlen(v) > 64 ? "at most 64 characters" : nullptr; }
const char* vHost(const char* v) {
	size_t n = strlen(v);
	if (n < 4 || n > 63 || !strchr(v, '.')) return "expected <project>.supabase.co";
	for (size_t i = 0; i < n; i++)
		if (!isalnum((unsigned char)v[i]) && v[i] != '.' && v[i] != '-') return "only letters, digits, '.' and '-'";
	return nullptr;
}
const char* vAnon(const char* v) {
	size_t n = strlen(v);
	if (n < 20 || n > 255) return "that does not look like a Supabase API key";
	for (size_t i = 0; i < n; i++)
		if (v[i] <= ' ' || v[i] == '"' || v[i] == '\\') return "must not contain spaces or quotes";
	return nullptr;
}
const char* vEmail(const char* v) { return (!strchr(v, '@') || strlen(v) > 63) ? "expected an email address" : nullptr; }
const char* vPassword(const char* v) { return strlen(v) > 63 ? "at most 63 characters" : nullptr; }
const char* vDoorId(const char* v) {
	size_t n = strlen(v);
	if (n < 1 || n > 31) return "1 to 31 characters";
	for (size_t i = 0; i < n; i++)
		if (!isalnum((unsigned char)v[i]) && v[i] != '_' && v[i] != '-') return "only letters, digits, '_' and '-'";
	return nullptr;
}
// Legacy tags were signed with whatever passphrase the old config.h had, so any length is accepted.
const char* vSecret(const char* v) { return (!*v || strlen(v) > 128) ? "1 to 128 characters" : nullptr; }
const char* vNtp(const char* v) { return (strlen(v) < 3 || strlen(v) > 39) ? "a host name" : nullptr; }

// Cleans a pasted URL down to a bare host.
void normalizeHost(char* v) {
	if (!strncmp(v, "https://", 8)) memmove(v, v + 8, strlen(v + 8) + 1);
	else if (!strncmp(v, "http://", 7)) memmove(v, v + 7, strlen(v + 7) + 1);
	char* slash = strchr(v, '/');
	if (slash) *slash = 0;
}

// Asks for one field. Enter keeps the current value, '-' clears an optional one. Returns false if the
// user went idle.
bool ask(const char* label, const char* help, char* target, size_t cap, Validator check, bool secret,
         bool optional = false, bool host = false) {
	char line[300];
	for (;;) {
		Serial.println();
		if (help && *help) { Serial.print(F("  ")); Serial.println(help); }
		Serial.print(label);
		if (target[0]) {
			Serial.print(F(" ["));
			if (secret) { Serial.print(F("hidden, ")); Serial.print(strlen(target)); Serial.print(F(" chars")); }
			else Serial.print(target);
			Serial.print(F(", Enter to keep]"));
		} else if (optional) Serial.print(F(" [empty]"));
		Serial.print(F(": "));

		int n = readLine(line, sizeof(line), kWizardIdleMs, true);
		if (n < 0) return false;
		if (n == 0) {
			if (target[0] || optional) return true;
			Serial.println(F("  This one is required."));
			continue;
		}
		if (optional && !strcmp(line, "-")) { target[0] = 0; return true; }
		if (host) normalizeHost(line);
		const char* err = check(line);
		if (err) { Serial.print(F("  Not accepted: ")); Serial.println(err); continue; }
		strlcpy(target, line, cap);
		return true;
	}
}

bool wizard(Settings& s) {
	Settings draft = s;
	if (!draft.ntpServer[0]) strlcpy(draft.ntpServer, "pool.ntp.org", sizeof(draft.ntpServer));
	if (!draft.doorId[0]) strlcpy(draft.doorId, "main", sizeof(draft.doorId));

	Serial.println(F("\n=== Doora setup ==="));
	Serial.println(F("Answer each question and press Enter. Secrets are stored in EEPROM, not in the firmware."));
	Serial.println(F("Idle for 3 minutes to cancel."));

	bool ok = ask("Wi-Fi network name (SSID)", "", draft.wifiSsid, sizeof(draft.wifiSsid), vSsid, false) &&
	          ask("Wi-Fi password", "Leave empty for an open network; type '-' to clear.", draft.wifiPass,
	              sizeof(draft.wifiPass), vWifiPass, true, true) &&
	          ask("Supabase host", "Project URL without https://, e.g. abcdefgh.supabase.co", draft.supabaseHost,
	              sizeof(draft.supabaseHost), vHost, false, false, true) &&
	          ask("Supabase publishable (anon) key", "Dashboard > Project Settings > API Keys.", draft.anonKey,
	              sizeof(draft.anonKey), vAnon, true) &&
	          ask("Door account email", "A Supabase user listed in public.door_users (see esp32/README.md).",
	              draft.email, sizeof(draft.email), vEmail, false) &&
	          ask("Door account password", "", draft.password, sizeof(draft.password), vPassword, true) &&
	          ask("Door id", "The id of this door in public.doors; 'main' unless you run several.", draft.doorId,
	              sizeof(draft.doorId), vDoorId, false) &&
	          ask("Key signing secret", "Equal to the keys edge function's JWT_SIGNING_KEY secret.", draft.jwtSecret,
	              sizeof(draft.jwtSecret), vSecret, true) &&
	          ask("NTP server", "", draft.ntpServer, sizeof(draft.ntpServer), vNtp, false);
	if (!ok) { Serial.println(F("\nSetup cancelled, nothing was changed.")); return false; }

	Serial.println(F("\nSave these settings? (y/n): "));
	char line[8];
	if (readLine(line, sizeof(line), kWizardIdleMs, true) < 0 || (line[0] != 'y' && line[0] != 'Y')) {
		Serial.println(F("Nothing was changed."));
		return false;
	}
	if (!SettingsStore::save(draft)) {
		Serial.println(F("Could not write the settings to EEPROM. Nothing was changed."));
		return false;
	}
	s = draft;
	Serial.println(F("Saved."));
	return true;
}

void showSettings(const Settings& s) {
	Serial.printf("wifi ssid : %s\nwifi pass : %s\nsupabase  : %s\nanon key  : hidden (%u chars)\n"
	                     "door user : %s\ndoor pass : hidden\ndoor id   : %s\nsecret    : hidden (%u chars)\nntp       : %s\n",
	                s.wifiSsid, s.wifiPass[0] ? "hidden" : "(open network)", s.supabaseHost, (unsigned)strlen(s.anonKey),
	                s.email, s.doorId, (unsigned)strlen(s.jwtSecret), s.ntpServer);
}

struct EraseTarget { const char* name; const char* what; Category cat; };
const EraseTarget kEraseTargets[] = {
	{"wifi", "Wi-Fi network and password", Category::Wifi},
	{"supabase", "Supabase host, key and door account", Category::Supabase},
	{"door", "door id and key signing secret", Category::Door},
	{"ntp", "NTP server", Category::Time},
};

// `erase [category|all]`. Returns true if settings changed (the caller restarts, and the setup
// wizard then asks only for what was erased).
bool eraseSettings(Settings& s, const char* arg) {
	while (*arg == ' ') arg++;
	bool all = !strcmp(arg, "all");
	const EraseTarget* target = nullptr;
	for (const EraseTarget& t : kEraseTargets) if (!strcmp(arg, t.name)) target = &t;
	if (!all && !target) {
		Serial.println(F("Usage: erase <category>, one of:"));
		for (const EraseTarget& t : kEraseTargets) Serial.printf("  %-9s %s\n", t.name, t.what);
		Serial.println(F("  all       everything above"));
		return false;
	}
	Serial.printf("Erase %s? Type 'yes' to confirm: ", all ? "ALL settings" : target->what);
	char answer[8];
	if (readLine(answer, sizeof(answer), 30000, true, 600) < 0 || strcmp(answer, "yes")) {
		Serial.println(F("Nothing was changed."));
		return false;
	}
	if (all) {
		SettingsStore::erase();
	} else {
		Settings next = s;
		SettingsStore::clear(next, target->cat);
		if (!SettingsStore::save(next)) {
			Serial.println(F("Could not write the settings to EEPROM. Nothing was changed."));
			return false;
		}
	}
	Serial.println(F("Erased. Restarting into setup."));
	return true;
}

}  // namespace

namespace Console {

void firstRunSetup(Settings& s) {
	if (!SettingsStore::loadRaw(s)) memset(&s, 0, sizeof(s));  // keep what survived a partial erase
	Serial.println(F("\nThis door is not configured yet."));
	// wizard() only returns true once the settings are validated and saved.
	while (!wizard(s)) {
		Serial.println(F("\nPress Enter to start the setup again."));
		char line[8];
		readLine(line, sizeof(line), 0, false);
	}
}

bool poll(Settings& s, void (*printStatus)()) {
	static char line[24];
	static size_t n = 0;
	static uint32_t lastByteMs = 0;
	// Monitors set to "no line ending" never send \r or \n; treat a pause after typing as Enter.
	bool idleFlush = n && !Serial.available() && millis() - lastByteMs > 400;
	while (idleFlush || Serial.available()) {
		int c = idleFlush ? '\n' : Serial.read();
		idleFlush = false;
		lastByteMs = millis();
		if (c != '\r' && c != '\n') {
			if (c >= 0x20 && n + 1 < sizeof(line)) line[n++] = (char)c;
			continue;
		}
		line[n] = 0;
		n = 0;
		// Tolerate monitors that add spaces or send upper case.
		char* cmd = line;
		while (*cmd == ' ') cmd++;
		for (char* e = cmd + strlen(cmd); e > cmd && e[-1] == ' ';) *--e = 0;
		for (char* p = cmd; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
		if (cmd != line) memmove(line, cmd, strlen(cmd) + 1);
		if (!line[0]) continue;
		if (!strcmp(line, "config") || !strcmp(line, "setup")) return wizard(s);
		if (!strcmp(line, "status")) printStatus();
		else if (!strcmp(line, "show")) showSettings(s);
		else if (!strcmp(line, "reboot")) { Serial.println(F("Restarting.")); delay(100); ESP.restart(); }
		else if (!strncmp(line, "erase", 5)) return eraseSettings(s, line + 5);
		else
			Serial.println(F("Commands:\n  status  network, clock, blacklist and journal state\n  show    current settings (secrets hidden)\n  config  run the setup wizard again\n  erase   wipe settings: erase [wifi|supabase|door|ntp|all]\n  reboot  restart the door"));
	}
	return false;
}

}  // namespace Console
