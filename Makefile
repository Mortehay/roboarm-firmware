# Default: every target runs inside the roboarm-firmware-tools Docker image.
# Use LOCAL=1 to run natively (needs arduino-cli, see scripts/setup-arduino-cli.sh).
export PATH := $(HOME)/.local/bin:$(PATH)
PORT ?= /dev/ttyUSB0
CLI  ?= arduino-cli
LOCAL ?=
IMAGE ?= roboarm-firmware-tools
ESP32_FQBN := esp32:esp32:esp32
UNO_FQBN   := arduino:avr:uno
CORE_SRCS  := $(wildcard roboarm/src/core/*.cpp)

.PHONY: test build-esp32 build-uno flash-esp32 flash-uno monitor image

image:
	docker build -t $(IMAGE) .

ifeq ($(LOCAL),1)

test:
	@mkdir -p build
	g++ -std=c++17 -Wall -Wextra -Werror -Iroboarm/src $(CORE_SRCS) $(wildcard test/*.cpp) -o build/tests
	./build/tests

build-esp32:
	$(CLI) compile --fqbn $(ESP32_FQBN) roboarm

build-uno:
	$(CLI) compile --fqbn $(UNO_FQBN) roboarm

flash-esp32: build-esp32
	$(CLI) upload -p $(PORT) --fqbn $(ESP32_FQBN) roboarm

flash-uno: build-uno
	$(CLI) upload -p $(PORT) --fqbn $(UNO_FQBN) roboarm

monitor:
	$(CLI) monitor -p $(PORT) -c baudrate=115200

else

# Build the image if it is missing.
ensure-image:
	@docker image inspect $(IMAGE) >/dev/null 2>&1 || $(MAKE) --no-print-directory image

# -it only when stdin is a terminal (monitor needs it; CI must not have it).
DOCKER_RUN = docker run --rm $$([ -t 0 ] && echo -it) \
	--user $$(id -u):$$(id -g) -e HOME=/tmp \
	-v "$(CURDIR)":"$(CURDIR)" -w "$(CURDIR)"
# Fail early with a clear message when the serial port is absent.
CHECK_PORT = test -e "$(PORT)" || { echo "error: serial port $(PORT) not found. Plug in the board and pass PORT=/dev/ttyXXX (see docs/flashing.md)." >&2; exit 1; }
DEVICE_ARGS = --device "$(PORT)" --group-add $$(stat -c %g "$(PORT)")

.PHONY: ensure-image

test build-esp32 build-uno: ensure-image
	$(DOCKER_RUN) $(IMAGE) make $@ LOCAL=1

flash-esp32 flash-uno monitor: ensure-image
	@$(CHECK_PORT)
	$(DOCKER_RUN) $(DEVICE_ARGS) $(IMAGE) make $@ LOCAL=1 PORT=$(PORT)

endif
