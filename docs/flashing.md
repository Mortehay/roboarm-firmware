# Flashing

**Servo PSU output OFF while flashing.** The board is powered from USB alone while you upload.

## 1. One-time toolchain setup

```
scripts/setup-arduino-cli.sh
```

It installs `arduino-cli` into `~/.local/bin` (if missing), the ESP32 and AVR cores, and the ESP32Servo and Servo libraries. It is safe to run again. The Makefile adds `~/.local/bin` to `PATH`.

## 2. Find the serial port

Unplug the board, then:

```
ls /dev/ttyUSB* /dev/ttyACM*
```

Plug the board in and run the same command again. The new entry is your port (usually `/dev/ttyUSB0` for ESP32 D1 R32 with a CH340 chip, `/dev/ttyACM0` for an Uno). `dmesg | tail` shows which device name the kernel assigned.

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

`PORT` defaults to `/dev/ttyUSB0`. Run `make test` for the host unit tests before flashing.

After a reset the monitor shows `READY RoboArm` (115200 baud). Servos stay limp until you send `H`. Before sending `H`, read the [pre-power checklist](wiring/README.md).

Monitor commands: `M j d` (move joint j to d degrees), `A d1 d2 d3 d4 d5`, `V s` (speed deg/s), `H` (home), `S` (stop), `R` (release), `?` (status).

## Troubleshooting

- **"Failed to connect to ESP32" / "Connecting........_____"**: hold the **BOOT** button on the board while the upload shows `Connecting...`, release once the upload starts. Also try a different USB cable (some are charge-only).
- **"port busy" / "Resource busy"**: close the serial monitor, the control app, or anything else using the port, then retry.
- **Port not listed**: check the cable, the brltty fix above, and `dmesg | tail`.
- **Uno upload fails with sync errors**: the HC-05 is probably still connected to D0/D1. Unplug it.
- **No `READY RoboArm` in the monitor**: press the board's reset button; confirm baud 115200. Over Bluetooth the line is only sent if a client was already connected, so send `?` instead.
