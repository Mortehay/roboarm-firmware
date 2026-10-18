# RoboArm firmware

Firmware for a 5-joint servo robot arm (Claw, Wrist, Elbow, Shoulder, Base) running on ESP32 or Arduino Uno, controlled over a simple `\n`-terminated ASCII serial/Bluetooth protocol. The desktop control app lives in the companion repo `roboarm-control`.

- Design spec: [docs/superpowers/specs/](docs/superpowers/specs/)
- Implementation plan: [docs/superpowers/plans/2026-10-18-roboarm-stage1.md](docs/superpowers/plans/2026-10-18-roboarm-stage1.md)
- Wiring: [docs/wiring/](docs/wiring/)
- Flashing: [docs/flashing.md](docs/flashing.md)

## Quick start

Install Docker (your user in the `docker` group) and `make`. Nothing else is needed.

```
make test
make flash-esp32 PORT=/dev/ttyUSB0
```

All `make` targets run inside the `roboarm-firmware-tools` Docker image, which is built automatically on first use (`make image` builds it explicitly; the first build downloads several GB). To run natively without Docker, see "Without Docker (LOCAL=1)" in [docs/flashing.md](docs/flashing.md).
