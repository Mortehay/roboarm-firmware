export PATH := $(HOME)/.local/bin:$(PATH)
PORT ?= /dev/ttyUSB0
CLI  ?= arduino-cli
ESP32_FQBN := esp32:esp32:esp32
UNO_FQBN   := arduino:avr:uno
CORE_SRCS  := $(wildcard roboarm/src/core/*.cpp)

.PHONY: test build-esp32 build-uno flash-esp32 flash-uno monitor

test:
	@mkdir -p build
	g++ -std=c++17 -Wall -Wextra -Werror -Iroboarm/src $(CORE_SRCS) $(wildcard test/*.cpp) -o build/tests
	./build/tests

build-esp32:
	$(CLI) compile --fqbn $(ESP32_FQBN) --build-property "compiler.cpp.extra_flags=-I$(CURDIR)/roboarm/src" roboarm

build-uno:
	$(CLI) compile --fqbn $(UNO_FQBN) --build-property "compiler.cpp.extra_flags=-I$(CURDIR)/roboarm/src" roboarm

flash-esp32: build-esp32
	$(CLI) upload -p $(PORT) --fqbn $(ESP32_FQBN) roboarm

flash-uno: build-uno
	$(CLI) upload -p $(PORT) --fqbn $(UNO_FQBN) roboarm

monitor:
	$(CLI) monitor -p $(PORT) -c baudrate=115200
