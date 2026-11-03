// The serial console: first-run setup wizard plus a few maintenance commands.
#pragma once
#include "Settings.h"

namespace Console {
// Blocks until the user has entered a complete, saved configuration. Used on first boot.
void firstRunSetup(Settings& s);

// Non-blocking; call every loop. `printStatus` is shown for the `status` command.
// Returns true if the settings were changed (the caller should restart).
bool poll(Settings& s, void (*printStatus)());
}  // namespace Console
