# Wiring changelog

Each schematic carries a title block: `Wiring v<N> · <board> · Stage <S> · <date> · firmware config: config_<board>.h`.
The wiring version `<N>` must equal `wiringVersion` in that board's config header; the firmware reports it via the `I` command.

## esp32-d1r32

- **v1** (2026-10-18, Stage 1): initial wiring. Servo signals on GPIO13, 14, 27, 25, 26. Min firmware 0.2.0.
- **v2** (2026-11-27, Stage 1): sixth servo wrist1 (J6, MG90) on GPIO17 (D4). Min firmware 0.3.0.

## uno

- **v1** (2026-10-18, Stage 1): initial wiring with HC-05 on D0/D1. Servo signals on D3, D5, D6, D9, D10. Min firmware 0.2.0.
- **v2** (2026-11-27, Stage 1): sixth servo wrist1 (J6, MG90) on D11. Min firmware 0.3.0.

## esp8266-d1

- **v1** (2026-10-18, Stage 1): initial wiring. Servo signals on GPIO13, 12, 14, 4, 5. Min firmware 0.2.0. Diagram: [esp8266-d1.svg](esp8266-d1.svg).
