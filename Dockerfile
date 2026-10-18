FROM ubuntu:24.04

ARG ARDUINO_CLI_VERSION=1.5.1
ARG ESP32_URL=https://espressif.github.io/arduino-esp32/package_esp32_index.json
ARG ESP8266_CORE_VERSION=3.1.2
ARG ESP8266_URL=https://arduino.esp8266.com/stable/package_esp8266com_index.json

ENV DEBIAN_FRONTEND=noninteractive \
    ARDUINO_DIRECTORIES_DATA=/opt/arduino/data \
    ARDUINO_DIRECTORIES_USER=/opt/arduino/user \
    ARDUINO_DIRECTORIES_DOWNLOADS=/tmp/arduino-downloads

RUN apt-get update \
 && apt-get install -y --no-install-recommends curl ca-certificates make g++ python3 \
 && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
    | BINDIR=/usr/local/bin sh -s ${ARDUINO_CLI_VERSION}

RUN mkdir -p "$ARDUINO_DIRECTORIES_DATA" "$ARDUINO_DIRECTORIES_USER" \
 && arduino-cli core update-index --additional-urls "$ESP32_URL,$ESP8266_URL" \
 && arduino-cli core install esp32:esp32@3.3.12 arduino:avr@1.8.8 esp8266:esp8266@${ESP8266_CORE_VERSION} --additional-urls "$ESP32_URL,$ESP8266_URL" \
 && arduino-cli lib install ESP32Servo@3.1.3 Servo@1.3.0 \
 && rm -rf "$ARDUINO_DIRECTORIES_DOWNLOADS" \
 && chmod -R a+rX /opt/arduino

# The ESP8266 core's build script writes CommonHFile.h into its own core dir;
# builds run as a non-root user, so that directory must be world-writable.
RUN chmod a+w "$ARDUINO_DIRECTORIES_DATA/packages/esp8266/hardware/esp8266/${ESP8266_CORE_VERSION}/cores/esp8266"

WORKDIR /work
