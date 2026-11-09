# Thin wrappers; PlatformIO (`pio`) does the real work. See platformio.ini.
.PHONY: build upload monitor test release

# Publishes the signed build for approval in the portal (see tools/release.py for the settings it needs).
release: build
	python3 tools/release.py

build:
	@test -f config.h || (echo "config.h is missing: copy config.example.h and set your wiring" && exit 1)
	pio run

upload: build
	pio run -t upload

# The setup wizard and console run at 115200 baud.
monitor:
	pio device monitor

# Host-side unit tests of the portable core (no board or toolchain needed).
test:
	$(MAKE) -C host_test test
