ARDUINO_CLI ?= arduino-cli

FQBN := esp32:esp32:esp32
CORE_VERSION := 3.2.1

SKETCH := firmware/box
LIBRARIES := $(abspath firmware/third_party)
BUILD_DIR := $(abspath build/box)

PORT ?= /dev/ttyUSB0

.PHONY: all build flash monitor clean versions install-core

all: build

build:
	$(ARDUINO_CLI) compile \
		--fqbn $(FQBN) \
		--libraries "$(LIBRARIES)" \
		--build-path "$(BUILD_DIR)" \
		--verbose \
		$(SKETCH)

flash: build
	$(ARDUINO_CLI) upload \
		--fqbn $(FQBN) \
		--port $(PORT) \
		--build-path "$(BUILD_DIR)" \
		$(SKETCH)

monitor:
	$(ARDUINO_CLI) monitor \
		--port $(PORT) \
		--config baudrate=115200

clean:
	rm -rf "$(BUILD_DIR)"

install-core:
	$(ARDUINO_CLI) core install esp32:esp32@$(CORE_VERSION)

versions:
	$(ARDUINO_CLI) version
	$(ARDUINO_CLI) core list
	grep -H '^version=' firmware/third_party/*/library.properties

