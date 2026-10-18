# Default: every target runs inside the roboarm-firmware-tools Docker image.
# Use LOCAL=1 to run natively (needs arduino-cli, see scripts/setup-arduino-cli.sh).
export PATH := $(HOME)/.local/bin:$(PATH)
PORT ?= /dev/ttyUSB0
CLI  ?= arduino-cli
LOCAL ?=
IMAGE ?= roboarm-firmware-tools
ESP32_FQBN := esp32:esp32:esp32
UNO_FQBN   := arduino:avr:uno
ESP8266_FQBN := esp8266:esp8266:d1
WIFI_SECRETS := roboarm/src/boards/wifi_secrets.h
GEN_SECRETS  = python3 scripts/gen_wifi_secrets.py .env $(WIFI_SECRETS)
CORE_SRCS  := $(wildcard roboarm/src/core/*.cpp)

.PHONY: test build-esp32 build-uno build-esp8266 flash-esp32 flash-uno flash-esp8266 monitor image

DF_HASH := $(shell sha256sum Dockerfile | cut -d' ' -f1)

image:
	docker build --label roboarm.dockerfile=$(DF_HASH) -t $(IMAGE) .

ifeq ($(LOCAL),1)

test:
	@mkdir -p build
	g++ -std=c++17 -Wall -Wextra -Werror -Iroboarm/src $(CORE_SRCS) $(wildcard test/*.cpp) -o build/tests
	./build/tests
	python3 -m unittest discover -s test/py

build-esp32:
	$(CLI) compile --fqbn $(ESP32_FQBN) roboarm

build-uno:
	$(CLI) compile --fqbn $(UNO_FQBN) roboarm

build-esp8266:
	$(GEN_SECRETS)
	$(CLI) compile --fqbn $(ESP8266_FQBN) roboarm

flash-esp32: build-esp32
	$(CLI) upload -p $(PORT) --fqbn $(ESP32_FQBN) roboarm

flash-uno: build-uno
	$(CLI) upload -p $(PORT) --fqbn $(UNO_FQBN) roboarm

flash-esp8266: build-esp8266
	$(CLI) upload -p $(PORT) --fqbn $(ESP8266_FQBN) roboarm

monitor:
	$(CLI) monitor -p $(PORT) -c baudrate=115200

else

# Build the image if it is missing or was built from a different Dockerfile.
ensure-image:
	@[ "$$(docker image inspect -f '{{index .Config.Labels "roboarm.dockerfile"}}' $(IMAGE) 2>/dev/null)" = "$(DF_HASH)" ] || $(MAKE) --no-print-directory image

# -it only when stdin is a terminal (monitor needs it; CI must not have it).
DOCKER_RUN = docker run --rm --init $$([ -t 0 ] && echo -it) \
	--user $$(id -u):$$(id -g) -e HOME=/tmp \
	-v "$(CURDIR)":"$(CURDIR)" -w "$(CURDIR)"
# Fail early with a clear message when the serial port is absent.
CHECK_PORT = test -e "$(PORT)" || { echo "error: serial port $(PORT) not found. Plug in the board and pass PORT=/dev/ttyXXX (see docs/flashing.md)." >&2; exit 1; }
DEVICE_ARGS = --device "$(PORT)" --group-add $$(stat -c %g "$(PORT)")

.PHONY: ensure-image check-port

check-port:
	@$(CHECK_PORT)

test build-esp32 build-uno build-esp8266: ensure-image
	$(DOCKER_RUN) $(IMAGE) make $@ LOCAL=1

# Port check first, so a missing device does not trigger an image build.
flash-esp32 flash-uno flash-esp8266 monitor: check-port ensure-image
	$(DOCKER_RUN) $(DEVICE_ARGS) $(IMAGE) make $@ LOCAL=1 PORT=$(PORT)

endif
