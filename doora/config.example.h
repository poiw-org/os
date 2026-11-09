// Wiring for your board. Copy this file to config.h (git-ignored) and set the pins (GPIO numbers).
//
// Nothing secret lives here: Wi-Fi, Supabase and the signing secret are entered over the serial
// console on first boot and stored in EEPROM (see README.md).
//
// The PN532 reader is wired to SDA 21 and SCL 22 unless you define NFC_SDA / NFC_SCL below. Avoid the strapping pins
// 0, 2, 5, 12 and 15 for outputs that are active at boot, such as the relay.

// PN532 I2C pins (the defaults, shown here to change them).
// #define NFC_SDA 21
// #define NFC_SCL 22

// Door relay output (HIGH = open) and the buzzer.
#define DOOR_PIN 25
#define SPEAKER 26

// Status LED, the red, green and blue of one RGB LED. BUSY (blue): reading a tag. ERROR (red): access
// denied. SUCCESS (green): door opening. All three daydream dimly when idle.
#define BUSY_LED 32
#define ERROR_LED 33
#define SUCCESS_LED 23

// Firmware updates over Bluetooth (optional; README.md, "Updates over Bluetooth").
// `python3 tools/ota_keygen.py` creates the private key in ~/.doora and prints this line, which is
// the public half of the key and not secret.
// #define OTA_SIGNING_KEY { 0x.., ... }
