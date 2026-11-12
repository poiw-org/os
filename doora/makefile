# Thin wrappers; PlatformIO (`pio`) does the real work. See platformio.ini.
.PHONY: build upload monitor test release

# The PlatformIO environment to build, flash or publish: `make release ENV=ble_spike`.
ENV ?= esp32dev

# Publishes the build for approval in the portal (see tools/release.py for the settings it needs).
release: build
	~/.platformio/penv/bin/python tools/release.py .pio/build/$(ENV)/firmware.bin

build:
	@test -f config.h || (echo "config.h is missing: copy config.example.h and set your wiring" && exit 1)
	pio run -e $(ENV)

upload: build
	pio run -e $(ENV) -t upload

# The setup wizard and console run at 115200 baud.
monitor:
	pio device monitor

# Host-side unit tests of the portable core (no board or toolchain needed).
test:
	$(MAKE) -C host_test test
