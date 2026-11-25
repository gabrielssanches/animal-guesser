ARDUINO_CLI ?= arduino-cli

FQBN := esp32:esp32:esp32
CORE_VERSION := 4.0.0

SKETCH := firmware/box
LIBRARIES := $(abspath firmware/third_party)
BUILD_DIR := $(abspath build/box)

DEBUG ?= 0
PORT ?= /dev/ttyUSB0

ifeq ($(DEBUG),1)
    BUILD_FLAGS := --build-property compiler.cpp.extra_flags=-DBOX_DEBUG=1
endif

.PHONY: all build flash monitor clean versions install-core

all: build

install-core:
	$(ARDUINO_CLI) config add board_manager.additional_urls \
		https://espressif.github.io/arduino-esp32/package_esp32_index.json
	$(ARDUINO_CLI) core update-index
	$(ARDUINO_CLI) core install esp32:esp32@$(CORE_VERSION)

build:
	$(ARDUINO_CLI) compile \
		--fqbn $(FQBN) \
		--libraries "$(LIBRARIES)" \
		--build-path "$(BUILD_DIR)" \
		$(BUILD_FLAGS) \
		--verbose \
		$(SKETCH)

flash:
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

versions:
	$(ARDUINO_CLI) version
	$(ARDUINO_CLI) core list
	grep -H '^version=' firmware/third_party/*/library.properties

