#!/usr/bin/env bash
# Idempotent toolchain setup: arduino-cli, ESP32 + ESP8266 + AVR cores, libraries.
set -euo pipefail

URL="https://espressif.github.io/arduino-esp32/package_esp32_index.json,https://arduino.esp8266.com/stable/package_esp8266com_index.json"
export PATH="$HOME/.local/bin:$PATH"

if ! command -v arduino-cli >/dev/null 2>&1; then
  mkdir -p "$HOME/.local/bin"
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
    | BINDIR="$HOME/.local/bin" sh
fi

arduino-cli version
arduino-cli core update-index --additional-urls "$URL"
arduino-cli core install esp32:esp32@3.3.12 arduino:avr@1.8.8 esp8266:esp8266@3.1.2 --additional-urls "$URL"
arduino-cli lib install ESP32Servo@3.1.3 Servo@1.3.0

echo "--- cores ---"
arduino-cli core list
echo "--- libs ---"
arduino-cli lib list
