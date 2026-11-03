// Wiring for your board. Copy this file to config.h (git-ignored) and set the pins.
//
// Nothing secret lives here any more: Wi-Fi, Supabase and the signing secret are entered over
// the serial console on first boot and stored in EEPROM (see README.md).

// Door relay output (HIGH = open) and the buzzer.
#define DOOR_PIN D0
#define SPEAKER D3

// Status LEDs. IDLE: online / blinking when offline. BUSY: reading a tag. ERROR: access denied.
// SUCCESS: door opening.
#define IDLE_LED D5
#define BUSY_LED D6
#define ERROR_LED D7
#define SUCCESS_LED D8
