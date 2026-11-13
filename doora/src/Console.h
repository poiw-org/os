// The serial console: first-run setup wizard plus a few maintenance commands.
#pragma once
#include "Settings.h"

namespace Console {
// Blocks until the user has entered a complete, saved configuration. Used on first boot.
void firstRunSetup(Settings& s);

// Non-blocking; call every loop. `printStatus` is shown for the `status` command. The `config`
// and `erase` prompts wait for input; `idle` is called all the while, so the door keeps working.
// Returns true if the settings were changed (the caller should restart).
// `extra` gets every other command first (lower case, trimmed) and returns true if it handled it;
// the speaker's `pitch` and `play` commands live there.
bool poll(Settings& s, void (*printStatus)(), void (*idle)(), bool (*extra)(const char*));

// Runs a command from the portal. Its own allowlist: nothing that prompts or wipes settings.
void remote(const char* command, const Settings& s, void (*printStatus)(), bool (*extra)(const char*));
}  // namespace Console
