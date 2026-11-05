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

// Updates over Wi-Fi (optional; README.md, "Updates over Wi-Fi"). `python3 tools/ota_keygen.py` creates
// the private key and password in ~/.doora and prints these two lines. Neither is secret: the door
// only gets the MD5 of the password and the public half of the signing key.
// #define OTA_PASSWORD_MD5 "..."
// #define OTA_SIGNING_KEY { 0x.., ... }
