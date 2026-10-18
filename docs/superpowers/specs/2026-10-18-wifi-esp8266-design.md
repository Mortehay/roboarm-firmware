# RoboArm — Wi-Fi control on the ESP8266 "D1" board

Date: 2026-10-18
Status: draft, awaiting approval
Extends: `2026-10-18-roboarm-design.md` (Stage 1) and the Docker workflow (branch `docker` in both repos).

## Why

The owner's "D1" board turned out to be an **ESP8266** (esptool: "This chip is ESP8266", CH340 USB, `/dev/ttyUSB0`), not an ESP32. ESP8266 has Wi-Fi and no Bluetooth. The owner chose to control the arm **over their home Wi-Fi** with this board rather than buy an HC-05 or an ESP32. The app must **find and connect to the arm on the home network**. Wi-Fi credentials come from `.env`.

## Goals

1. Firmware build for ESP8266 D1 that joins the home Wi-Fi and accepts the existing text protocol over TCP. USB serial keeps working at the same time.
2. The control app discovers the arm on the LAN and connects over TCP, with the same controls and safety behaviour as the serial link.
3. Everything is driven by `make`, in Docker on Linux, as with the rest of the project. Secrets never reach git.

## Non-goals

- Access-point fallback mode. The owner chose home Wi-Fi only. Recovery is via USB serial: the IP is printed on boot and serial commands still work.
- TLS/encryption. This is a home LAN, protected by a shared key (below).
- Changing the ESP32/Bluetooth or Uno builds. They stay as they are.
- OTA firmware updates (Stage 2 at most).

## Anti-goals

- Wi-Fi password or arm key committed to the public repos, baked into a Docker image, or printed in logs.
- Any device on the LAN moving the arm without the key.
- A dropped Wi-Fi link making the arm go limp (it must hold position, as with Bluetooth).
- Servo pins on ESP8266 boot-strapping pins (GPIO 0, 2, 15), so the board never fails to boot with servos attached.

## Hardware (decided / assumed)

- Board: Uno-form-factor "WeMos D1" (ESP8266, 4 MB flash assumed), FQBN `esp8266:esp8266:d1` (R1) or `esp8266:esp8266:d1_mini`-compatible. *My call:* `esp8266:esp8266:d1`. The builder verifies against the board package and the owner's silkscreen. The exact R1/R2 revision is unknown; pins are specified by **GPIO number**.
- 3.3 V logic, like the ESP32. Servo power rules are unchanged (bench PSU, common ground, never to board 5V/VIN).
- Servo pins (*my call*). These are the only five ESP8266 GPIOs safe for servo PWM: J1 Claw GPIO13, J2 Wrist GPIO12, J3 Elbow GPIO14, J4 Shoulder GPIO4, J5 Base GPIO5. Never 0, 2, 15 (boot straps), 1, 3 (USB serial) or 16 (no PWM). Expected header labels on a D1 R2: GPIO13 = D7, GPIO12 = D6, GPIO14 = D5, GPIO4 = D4, GPIO5 = D3. The diagram marks these "verify on your board's silkscreen".
- The existing joint limits, home pose, speeds, homing order and arming model are unchanged (shared `core/`).

## Firmware

- New board branch in `roboarm.ino` for `ESP8266` (alongside ESP32 and Uno): `config_esp8266.h` (pins above), ESP8266 core `Servo` library, `ESP8266WiFi`, `ESP8266mDNS`, `WiFiUdp`.
- **Wi-Fi join:** station mode with credentials from a generated header. Retries forever in the background. Servo control over USB serial works while Wi-Fi is down. On connect, `WIFI <ip>` is printed on USB serial.
- **TCP server on port 3333.** One client at a time; a second client is refused with `ERR busy` and closed.
  - The first line from a client must be `K <ARM_KEY>`. Wrong or missing key → `ERR bad key`, then close.
  - After the key, the client gets `READY RoboArm`, and lines go through the same `LineReader` → `Arm::handleLine`, with replies to that client.
- **Client disconnect / Wi-Fi drop:** hold position (no detach), same as the Bluetooth rule.
- **Discovery:**
  - mDNS hostname `roboarm` (`roboarm.local`), service `_roboarm._tcp` on port 3333.
  - A UDP responder on port 3334: a datagram `ROBOARM?` gets the reply `ROBOARM <name> <ip> 3333`. The key is not needed for discovery and is never revealed.
- **Secrets:**
  - `.env` keys: `WIFI_SSID`, `WIFI_PASSWORD`, `ARM_KEY`.
  - `make` generates `roboarm/src/boards/wifi_secrets.h` from `.env` before an ESP8266 build. The file is gitignored, and a committed `wifi_secrets.h.example` serves Arduino IDE users.
  - The build fails with a clear message if any key is missing.
  - Values are escaped for C string literals. Secrets are never echoed by make.
- `make build-esp8266`, `make flash-esp8266 PORT=/dev/ttyUSB0`, `make monitor` (unchanged). The Docker image adds the `esp8266:esp8266` core (pinned version) via `https://arduino.esp8266.com/stable/package_esp8266com_index.json`. Secrets are passed into the container only through the bind-mounted generated header, never as image layers or build args.
- Native tests (`make test`) still cover the core. Any new pure logic (e.g. key check, UDP reply formatting) goes in Arduino-free code with tests.

## Control app

- **New link `TcpLink(host, port=3333, key)`** implementing the existing `Link` protocol (`connected`, `send`, `read_lines`, `close`):
  - connect timeout 3 s
  - sends `K <key>` first, and treats `ERR bad key` / `ERR busy` as a connection failure with that message
  - reader thread, line splitting and buffer cap as in `SerialLink`
  - write timeout, so a stalled Wi-Fi link can't freeze the GUI
  - a dropped socket → `connected = False` (the controller already handles this safely)
- **`open_link(port)`** accepts `tcp://<host>[:port]` (host = IP or `roboarm.local`) in addition to serial paths and `fake`. The key comes from env `ARM_KEY`, which make loads from `.env`. The GUI never shows it.
- **Discovery:**
  - `discover(timeout=1.0) -> list[tuple[name, ip, port]]` broadcasts `ROBOARM?` on UDP 3334 and collects replies.
  - The GUI gets a **"Find arm"** button that fills the port box with `tcp://<ip>:3333` entries. The port box still accepts typed values.
- **make targets:**
  - `make run-wifi`: GUI in Docker with `--network host`, needed for UDP broadcast discovery. It passes `ARM_KEY` from `.env` as an env var and does not bake it into the image.
  - `make run-wifi HOST=192.168.1.42`: preselects `tcp://HOST:3333`.
  - `LOCAL=1` variants as before.
- Everything else is unchanged: STOP/resync, homing, playback, jog pacing. The controller is transport-agnostic, so the existing tests stay valid. New tests cover `TcpLink` (fake socket) and `discover` (fake UDP socket).

## Docs

- Firmware: `docs/wiring/esp8266-d1.svg` + a pin table and the same pre-power checklist; `docs/flashing.md` ESP8266 section (FQBN, port, `.env` keys, "Failed to connect" → hold FLASH/reset).
- Control README: Wi-Fi quick start: fill `.env` → `make flash-esp8266` (firmware repo) → `make run-wifi` → Find arm → Connect → Home.

## Clarifications (controller's calls after builder review)

1. **Key threat model:** the key protects against casual LAN guests only. It travels in plaintext, accepted for a home LAN. After a bad key the firmware waits 1 s, then closes. No lockout. The key is compared with a simple constant-time loop.
2. **Silent links:**
   - The app sends `?` at least every 2 s while connected over TCP (a heartbeat using the existing command, so the protocol doesn't change).
   - The firmware drops a TCP client that has been silent for 10 s. It holds position, never detaches, and frees the slot. A new client is refused with `ERR busy` only while the current client is alive.
   - `TcpLink` enables TCP keepalive and treats 6 s with no data received as a dropped link (`connected = False`).
3. **USB and TCP:**
   - Both feed the same `Arm`, exactly like the ESP32 build's USB + Bluetooth. Each command's reply goes to the source that sent it.
   - Arming and homing state is shared, and any source's `S` is honoured.
   - USB serial needs no key (physical access).
   - Unsolicited lines (`WIFI <ip>`, boot `READY`) go to USB serial. `READY RoboArm` is also sent to a TCP client after a valid key.
   - No core refactor: `Arm::handleLine(line, ReplySink&)` already routes replies per source.
4. **Servo timing:**
   - The ESP8266 core `Servo` (waveform generator) is accepted for Stage 1. The owner judges any jitter on hardware.
   - `loop()` must never block: non-blocking Wi-Fi/mDNS/UDP/TCP servicing, `yield()`-friendly, and no `delay()` except the 1 s bad-key wait, which is done without blocking (close scheduled by timestamp).
   - Homing and playback timing stay `millis()`-based as today.
5. **Discovery fallbacks:**
   - `discover()` sends to 255.255.255.255 and to each local interface's subnet broadcast. If nothing answers, it tries to resolve `roboarm.local`.
   - If everything fails, the GUI says "No arm found — type its IP (shown on USB serial as `WIFI <ip>`)", and the port box accepts `tcp://<ip>`.
   - `LOCAL=1` runs natively on the host network.
6. **ESP8266 core:** no core is installed locally. Pin the latest stable `esp8266:esp8266` 3.1.x (the builder records the exact version) in the setup script and Dockerfile.
7. **Missing key in the app:** if `ARM_KEY` is empty or unset, `TcpLink` refuses to connect with "ARM_KEY not set (add it to .env)".
8. **Secret formats and parsing:**
   - `ARM_KEY` must match `[A-Za-z0-9_-]{8,64}`. `WIFI_SSID` is 1–32 bytes and `WIFI_PASSWORD` 8–63 bytes, any printable characters.
   - A small generator script parses `.env` itself (`KEY=value` lines; optional surrounding quotes stripped; no shell expansion) rather than `include .env` in make. It validates the values and writes the C header with escaping.
   - The app's make targets read `ARM_KEY` via the same parser and pass it as `-e ARM_KEY=…` to Docker. No `--env-file`.
   - Single source of secrets: the firmware repo's `.env`. The control repo's make looks for `ARM_KEY` in its own `.env` first, then in `$(ROBOARM_APP_PATH)/.env` (its `.env` already holds `ROBOARM_APP_PATH`).
9. **Owner's network:** `Tenda_8AA590` is a 2.4 GHz network the ESP8266 can join. Its `_5G` twin can't be used.

## Schematic and protocol versioning (owner request: "don't forget schema design versioning")

Interpreted as versioning the wiring schematics, tied to what the firmware reports, so the wiring on the bench can always be matched to the flashed firmware.

- **Wiring version per board:** an integer `kWiringVersion` in each `config_<board>.h`.
  - Existing ESP32 and Uno diagrams are **v1**. The new ESP8266 diagram is **v1**.
  - Any change to pins, power or added hardware (e.g. Stage 2 PCA9685) bumps that board's number.
- **Title block on every SVG:** `Wiring v<N> · <board> · Stage <S> · <date> · firmware config: config_<board>.h`.
  - Old versions are kept as `docs/wiring/archive/<board>.v<N>.svg` when superseded.
  - `docs/wiring/CHANGELOG.md` lists each version: what changed, why, and the minimum firmware version.
- **Firmware identity:** `kFirmwareVersion "0.2.0"` (semver; this Wi-Fi release) and `kProtocolVersion 1` in core.
  - New command `I` → `INFO RoboArm fw=0.2.0 proto=1 board=<esp32-d1r32|uno|esp8266-d1> wiring=<N>`. It works in any state and is answered like `?`.
  - Boot prints the same `INFO` line on USB.
- **App:** on connect it sends `I`, logs the INFO line, and shows `fw / board / wiring vN` in the status bar.
  - If `proto` ≠ 1, it shows a warning ("firmware protocol vN not supported by this app") and keeps the controls disabled.
  - If `I` gets `ERR bad command` (old firmware), it treats the firmware as proto 1 and wiring unknown, and logs "firmware too old to report wiring version".
  - FakeArm answers `INFO RoboArm fw=sim proto=1 board=sim wiring=0`.
- The rule is written into `docs/wiring/README.md`: "Before powering on, check the wiring version shown in the app matches the version in the diagram's title block."

## Decided by the owner

- Use the ESP8266 D1 over home Wi-Fi (not AP mode, not HC-05/ESP32).
- The app must discover and connect to the arm on the home network.
- The Wi-Fi password is provided in `.env`.

## Left to the builder

File layout inside the existing structure; the exact ESP8266 core version (pin it); the TCP/UDP code structure; the GUI placement of "Find arm"; and the test approach for networking (fakes, no real network in unit tests).
