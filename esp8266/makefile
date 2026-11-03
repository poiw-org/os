FQBN = esp8266:esp8266:nodemcuv2
PORT ?= /dev/ttyUSB0
RUN = nix-shell --pure --run

# arduino-cli picks up ./arduino-cli.yaml, which keeps the board package and libraries in ./arduino.

# One-time: board package and libraries. The firmware itself only needs ArduinoJson and the PN532
# reader libraries; TLS, Wi-Fi, EEPROM and the flash filesystem come with the ESP8266 core.
install:
	$(RUN) "arduino-cli core update-index"
	$(RUN) "arduino-cli core install esp8266:esp8266"
	$(RUN) "arduino-cli lib install ArduinoJson@7"
	rm -rf arduino/user/libraries/PN532 arduino/user/libraries/PN532_I2C arduino/user/libraries/NDEF
	git clone --depth 1 https://github.com/elechouse/PN532.git arduino/pn532-src
	mv arduino/pn532-src/PN532 arduino/pn532-src/PN532_I2C arduino/pn532-src/NDEF arduino/user/libraries/
	rm -rf arduino/pn532-src

update: install

# Host-side unit tests of the portable core (no board or toolchain needed).
test:
	$(MAKE) -C test test

compile:
	@test -f config.h || (echo "config.h is missing: copy config.example.h and set your wiring" && exit 1)
	$(RUN) "arduino-cli compile --fqbn $(FQBN) --libraries arduino/user/libraries/ --output-dir build/ ."

upload:
	$(RUN) "arduino-cli upload -p $(PORT) --fqbn $(FQBN) --input-dir build/"

# The setup wizard and console run at 115200 baud.
monitor:
	$(RUN) "arduino-cli monitor -p $(PORT) --fqbn $(FQBN) --config baudrate=115200"

.PHONY: install update test compile upload monitor
