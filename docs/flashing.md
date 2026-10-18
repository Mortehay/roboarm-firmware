# Flashing

**Servo PSU output OFF while flashing.** The board is powered from USB alone while you upload.

## 1. Setup

Install Docker (add your user to the `docker` group, then log out and in) and `make`. That is all. The first `make` command builds the `roboarm-firmware-tools` image (arduino-cli, ESP32 and AVR cores, libraries; several GB, takes a few minutes). `make image` builds it explicitly. Every `make` target runs inside this container; the flash and monitor targets pass the serial port through to it.

Quick path: `make test`, then `make flash-esp32 PORT=/dev/ttyUSB0`.

## 2. Find the serial port

Unplug the board, then:

```
ls /dev/ttyUSB* /dev/ttyACM*
```

Plug the board in and run the same command again. The new entry is your port (usually `/dev/ttyUSB0` for ESP32 D1 R32 with a CH340 chip, `/dev/ttyACM0` for a genuine Uno; Uno clones with a CH340 chip show up as `/dev/ttyUSB0`). `dmesg | tail` shows which device name the kernel assigned.

If you get "Permission denied" opening the port, add yourself to the serial group (`sudo usermod -aG dialout $USER`, then log out and in).

**CH340 disconnects right after plugging in:** the `brltty` package grabs these devices. Run:

```
sudo apt remove brltty
```

Then unplug and replug the board.

## 3. Build, flash, monitor

ESP32:

```
make build-esp32
make flash-esp32 PORT=/dev/ttyUSB0
make monitor PORT=/dev/ttyUSB0
```

Arduino Uno (**unplug the HC-05 from D0/D1 first**, and plug it back in after the upload finishes):

```
make build-uno
make flash-uno PORT=/dev/ttyACM0
make monitor PORT=/dev/ttyACM0
```

`PORT` defaults to `/dev/ttyUSB0`. In Docker mode the target fails immediately if the port does not exist. Run `make test` for the host unit tests before flashing.

After a reset the monitor shows `READY RoboArm` (115200 baud). Servos stay limp until you send `H`. Before sending `H`, read the [pre-power checklist](wiring/README.md).

Monitor commands: `M j d` (move joint j to d degrees), `A d1 d2 d3 d4 d5`, `V s` (speed deg/s), `H` (home), `S` (stop), `R` (release), `?` (status).

## ESP8266 D1 (Wi-Fi)

Wi-Fi credentials live in `.env` in the repo root (gitignored; never commit it). Add three keys:

```
WIFI_SSID=YourNetworkName
WIFI_PASSWORD=your-wifi-password
ARM_KEY=3f9a1c0b7d2e4a6c8b1d5e90
```

- `WIFI_SSID` / `WIFI_PASSWORD`: your router's 2.4 GHz network. The ESP8266 has no 5 GHz radio, so use your 2.4 GHz network, **not** the `_5G` one.
- `ARM_KEY`: shared secret the control app must send to connect. 8 to 40 characters: letters, digits, `_` and `-`. Generate one with `openssl rand -hex 12`.

The credentials are compiled into the firmware (a generated, gitignored header). Then, with the **servo PSU output OFF**:

```
make flash-esp8266 PORT=/dev/ttyUSB0
make monitor PORT=/dev/ttyUSB0
```

The board has a CH340 USB chip (same brltty fix as above). The monitor shows `READY RoboArm`, then `INFO RoboArm fw=0.2.0 proto=1 board=esp8266-d1 wiring=1`, then `WIFI <ip>` once it joins the network.

Quick network check from the laptop (same Wi-Fi):

```
printf 'ROBOARM?' | nc -u -w1 -b 255.255.255.255 3334
```

Expected reply: `ROBOARM RoboArm <ip> 3333`. Then connect and authenticate:

```
nc <ip> 3333
K <your key>        (reply: READY RoboArm)
?                   (reply: the STATE line)
```

Only one client at a time; a second gets `ERR busy`. A wrong key gets `ERR bad key`.

If the Wi-Fi password or SSID is wrong or the router is off, the arm keeps retrying every 15 s and USB control still works meanwhile.

## Without Docker (LOCAL=1)

To run natively, install the toolchain once:

```
scripts/setup-arduino-cli.sh
```

It installs `arduino-cli` into `~/.local/bin` (if missing), the ESP32 and AVR cores, and the ESP32Servo and Servo libraries. It is safe to run again. The Makefile adds `~/.local/bin` to `PATH`. Then prefix any target with `LOCAL=1`, e.g. `make LOCAL=1 flash-esp32 PORT=/dev/ttyUSB0`. Native builds also need `g++` and `make`.

## Troubleshooting

- **"Failed to connect to ESP32" / "Connecting........_____"**: hold the **BOOT** button on the board while the upload shows `Connecting...`, release once the upload starts. Also try a different USB cable (some are charge-only).
- **"port busy" / "Resource busy"**: close the serial monitor, the control app, or anything else using the port, then retry.
- **Port not listed**: check the cable, the brltty fix above, and `dmesg | tail`.
- **"Failed to connect" on the ESP8266**: unplug the servo signal wires and retry, pressing the board's RESET button once as the upload starts. The boot-strapping GPIOs are not used, but connected servos can still load the lines. Also try another USB cable.
- **Uno upload fails with sync errors**: the HC-05 is probably still connected to D0/D1. Unplug it.
- **No `READY RoboArm` in the monitor**: press the board's reset button; confirm baud 115200. Over Bluetooth the line is only sent if a client was already connected, so send `?` instead.
