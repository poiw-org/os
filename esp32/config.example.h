// Wiring for your board. Copy this file to config.h (git-ignored) and set the pins (GPIO numbers).
//
// Nothing secret lives here: Wi-Fi, Supabase and the signing secret are entered over the serial
// console on first boot and stored in EEPROM (see README.md).
//
// The PN532 reader uses the default I2C pins (SDA 21, SCL 22). Avoid the strapping pins
// 0, 2, 5, 12 and 15 for outputs that are active at boot, such as the relay.

// Door relay output (HIGH = open) and the buzzer.
#define DOOR_PIN 25
#define SPEAKER 26

// Status LEDs. IDLE: online / blinking when offline. BUSY: reading a tag. ERROR: access denied.
// SUCCESS: door opening.
#define IDLE_LED 27
#define BUSY_LED 32
#define ERROR_LED 33
#define SUCCESS_LED 23
