# RoboArm — Stage 1 Design (Bluetooth control, no driver board)

Date: 2026-10-18
Status: draft, awaiting approval

## Intent

A 3D-printed 5-servo robot arm should be controllable wirelessly from a laptop (now) and an Android phone (later). Stage 1 proves the whole chain — wiring, power, firmware, Bluetooth link, desktop control app — with the simplest hardware that is still safe. Stage 2 upgrades the hardware (servo driver board, proper PSU) and the link (BLE, Android app) without rewriting the apps.

The owner explicitly wants: wiring schematics that prevent burning anything, help connecting to and flashing the board, two separate apps in two public GitHub repos, and the work broken into Plane tasks.

## Goals (Stage 1)

1. Wiring diagrams + pre-power safety checklist for ESP32 D1 R32 (primary) and Arduino Uno + HC-05 (alternative).
2. Firmware that drives all 5 servos with joint limits, smooth motion, safe homing, and a text command protocol over Bluetooth Classic (SPP) and USB serial.
3. A desktop control app (Linux laptop) matching the approved sketch below.
4. A flashing guide + scripts so the owner can build and upload from the terminal.
5. Two public GitHub repos and a Plane plan with modules.

## Non-goals (Stage 1)

- Servo driver board (PCA9685), dedicated PSU, battery power — Stage 2.
- BLE, Android app — Stage 2.
- Inverse kinematics / Cartesian control, camera, autonomy.
- Position feedback (hobby servos give none).

## Anti-goals (failure even if it "works")

- Anything that routes servo current through the Arduino/ESP32 5V pin or USB.
- Arm slamming to an angle at full speed on power-up or command (breaks printed parts).
- Joints driven past mechanical limits.
- Plane API key (currently in `.mcp.json`) ending up in a public repo.
- Protocol tied to Bluetooth Classic so that BLE later means rewriting the apps.

## Hardware (as decided)

| # | Joint (owner's numbering) | Servo | Notes |
|---|---|---|---|
| 1 | Claw open/close | MG90 (micro, metal gear) | 4.8–6.0 V |
| 2 | Wrist — claw up/down | MG90 | 4.8–6.0 V |
| 3 | Arm segment, upper half ("elbow") | MG996 | 4.8–7.2 V, up to ~2.5 A stall |
| 4 | Arm segment, lower half ("shoulder") | MG996 | heaviest load |
| 5 | Base rotation | MG996 | |

*Elbow/shoulder labels for 3 and 4 are my guess at the owner's description (3 = half nearer the claw). Labels are cosmetic; numbering is the owner's and is used in the protocol.*

- **Primary board:** ESP32 "D1 R32" (Uno form factor, built-in Bluetooth). 3.3 V logic — MG90/MG996 accept 3.3 V signal.
- **Alternative board:** Arduino Uno + HC-05 module (owner does not yet own an HC-05; documented, not required for Stage 1 success).
- **Servo power (testing):** Wanptek GA305H bench supply (0–30 V, 0–5 A).
- **Board power:** USB from the laptop (also used for flashing).

### Power rules (must appear in diagrams and checklist)

- Bench PSU set to **6.0 V max** (MG90 limit), start at 5.5 V; current limit **3 A** for first tests, raise toward 5 A once motion is verified.
- PSU + / − → servo red / brown via a common rail (breadboard power rail or terminal block). **PSU never connects to the board's 5V/VIN.**
- **Common ground:** PSU − tied to board GND. Without it signals don't work.
- *My call:* a 470–1000 µF electrolytic capacitor (≥10 V) across the servo rail, observing polarity, to absorb current spikes.
- Set voltage and current limit on the PSU **before** connecting the servo rail; turn PSU output on **after** the board is running firmware, then press Home in the app (servos stay limp until then — see arming model).
- Five MG996-class moves at once can exceed 5 A → PSU current-limits → servos brown out. Firmware mitigates (staggered homing, speed limiting); real fix is Stage 2 PSU.

### Pin mapping

ESP32 D1 R32 (*my call*; builder must verify against the board's silkscreen / pinout before the diagram is finalized):

| Joint | GPIO | D1 R32 header label (expected) |
|---|---|---|
| 1 Claw | 13 | D9 |
| 2 Wrist | 14 | D7 |
| 3 Elbow | 27 | D6 |
| 4 Shoulder | 25 | D3 |
| 5 Base | 26 | D2 |

Avoid strapping/boot pins (GPIO 0, 2, 12, 15) and input-only pins (34, 35, 36, 39).

Arduino Uno: any five digital pins for servos (e.g. D3, D5, D6, D9, D10). HC-05: **HC-05 RX needs a voltage divider** (e.g. 1 kΩ / 2 kΩ) from the Uno's 5 V TX; HC-05 VCC to Uno 5V (module draws little), GND common. Whether the HC-05 goes on hardware serial (D0/D1, must be unplugged while flashing) or SoftwareSerial (can cause servo jitter with the Servo library) is left to the builder — document the trade-off and the choice.

## Command protocol (my call)

Line-based ASCII, `\n`-terminated, identical over USB serial (115200 baud) and Bluetooth SPP. Transport-agnostic so BLE (Stage 2) only swaps the transport layer.

Commands (app → arm):

| Command | Meaning |
|---|---|
| `M <joint> <deg>` | Move one joint toward angle at current speed |
| `A <d1> <d2> <d3> <d4> <d5>` | Move all joints (one pose) |
| `V <deg_per_s>` | Set motion speed |
| `H` | Attach (if detached) and go to home pose, staggered one joint at a time |
| `S` | Stop: freeze all joints where they are (stay powered/holding) |
| `R` | Release: detach all servos (go limp) |
| `?` | Report state |

Replies (arm → app): `OK ...`, `WARN <joint> clamped to <deg>`, `ERR <reason>`, `STATE <d1>..<d5> <attached 0/1> <speed>`.

*My call, differs from the approved sketch:* the sketch had one "STOP – release all" button. Releasing makes the arm collapse under its own weight, so STOP = freeze and a separate Release button detaches. The builder should keep both in the UI.

## Firmware (`roboarm-firmware`, this folder: `/home/markunn/Arduino/RoboArm`)

- Arduino sketch, built with `arduino-cli`. ESP32: `ESP32Servo` library + `BluetoothSerial` (device name `RoboArm`). Uno: `Servo` library + HC-05.
- Single config section per board: pins, per-joint min/max, home pose, default speed.
- **Initial limits are conservative** (e.g. 30–150°) until calibrated; calibration = jog each joint in the app, note safe extremes, write them into config. Document the procedure.
- **Arming model (my call):** servos are **detached at boot and after `R`**. Nothing moves until the owner, with the PSU output on, sends `H`, which attaches and homes **one joint at a time**, slowly. `M`/`A` while detached → `ERR not armed`. Rationale: the firmware cannot sense servo power or real joint positions, so auto-homing at boot would make all servos jump at once when the PSU is switched on later. (Hobby servos still jump to the first commanded position on attach; choose home ≈ the arm's natural resting pose to minimise that jump.)
- ESP32 boards reset when a USB serial port is opened; the control app should deassert DTR/RTS on open, and the arming model above makes an accidental reset harmless (arm stays detached).
- Motion: non-blocking interpolation toward targets at configured speed; commands accepted mid-motion.
- Out-of-range commands are clamped and reported with `WARN`.
- Bluetooth disconnect: hold position (do not release — arm would fall).
- Accept commands on USB serial and Bluetooth simultaneously.

## Control app (`roboarm-control`, `/home/markunn/Arduino/RoboArmControl`)

Approved sketch:

```
┌─ RoboArm Control ─────────────────────────────────────────┐
│ Port: [/dev/rfcomm0 ▾]  [Connect]   ● Connected  ESP32-Arm│
├───────────────────────────────────────────────────────────┤
│ 5 Base      ◄ ──────────●────────────── ►  90°   [Q] [A]  │
│ 4 Shoulder  ◄ ────────●──────────────── ►  80°   [W] [S]  │
│ 3 Elbow     ◄ ──────────────●────────── ► 110°   [E] [D]  │
│ 2 Wrist     ◄ ──────────●────────────── ►  90°   [R] [F]  │
│ 1 Claw      ◄ ●──────────────────────── ►  10°   [T] [G]  │
├───────────────────────────────────────────────────────────┤
│ Speed: [slow|•med|fast]                                   │
│ [Home pose]   [■ STOP]  [Release]   [Save pose] [Play]    │
│ Poses: Home · Pick · Drop                                 │
├───────────────────────────────────────────────────────────┤
│ log: > M 3 110   < OK 3 110                               │
└───────────────────────────────────────────────────────────┘
```

- Python 3.12 + `pyserial`. GUI toolkit: *my call* Tkinter (stdlib, minimal install; `python3-tk` apt package may be needed).
- Connects to any serial port: `/dev/rfcomm0` (Bluetooth Classic bound via `bluetoothctl` + `rfcomm`) or `/dev/ttyUSB0` (USB cable). Port list + manual entry.
- Sliders limited to each joint's configured range; hold-key jogging; Space = STOP.
- Poses saved as named JSON; Play steps through a list of poses.
- Protocol/client logic separated from the GUI and unit-tested; a fake-arm (mock serial) mode so the app can be developed without hardware.
- README: how to pair the ESP32 and bind `/dev/rfcomm0` on Linux.

## Flashing (current machine state, verified 2026-10-18)

- Arduino IDE ESP32 core already present in `~/.arduino15/packages/esp32`; `arduino-cli` **not installed** — install it.
- User is in `dialout` (serial access OK). No board was connected at check time.
- Guide must cover: install `arduino-cli`, cores (`esp32:esp32`, `arduino:avr`), libraries; identify the port (`/dev/ttyUSB0` for CH340/CP210x boards, `/dev/ttyACM0` for genuine Uno); compile + upload commands (FQBN `esp32:esp32:esp32` for D1 R32, `arduino:avr:uno`); troubleshooting ("Failed to connect" → hold BOOT during upload; port busy; CH340 visibility; brltty grabbing CH340 on Ubuntu).
- Provide scripts (e.g. `make flash-esp32`, `make flash-uno` or shell equivalents).
- Flashing does not require servo power; recommend PSU output off while flashing.

## Wiring diagrams

In `roboarm-firmware/docs/wiring/`: one for ESP32 D1 R32, one for Uno + HC-05. Format left to builder (Fritzing/KiCad/SVG all acceptable) but each must be viewable on GitHub as an image and accompanied by a pin table and the pre-power checklist (voltage set, current limit set, common ground, polarity, no PSU→board 5V, capacitor polarity, PSU output off until firmware runs).

## Repos & secrets

- GitHub account `Mortehay` (gh CLI authenticated). **Public** repos: `roboarm-firmware` (this folder), `roboarm-control` (`RoboArmControl`).
- Add `.mcp.json` to `.gitignore` in **both** repos before their first commit (both folders have one containing the Plane API key); provide `.mcp.json.example` without the key.
- `.env`: `CONTROL_APP_PATH` = control app folder, `ROBOARM_APP_PATH` = firmware folder (the owner's naming). Add `ROBOARM_APP_PATH=/home/markunn/Arduino/RoboArm/` (currently missing; existing line lacks trailing newline).

## Plane plan (workspace `roboarm`)

One Plane project **RoboArm** (create if absent; *my call*) covering both repos, with modules:
1. **Control App**
2. **Firmware & Flashing** (includes wiring diagrams, flashing setup, calibration)
3. **Stage 2** (backlog): PCA9685 driver board, dedicated 6 V ≥10 A PSU (or 2S LiPo + 6 V UBEC), BLE transport, Android app.

Plane MCP is configured in `.mcp.json` but was not loaded in the session that wrote this; it must be enabled before tasks can be created.

## Stage 2 (recorded, not designed)

PCA9685 16-channel servo board over I²C (offloads PWM, separate V+ terminal for servo power); dedicated 6 V ≥10 A PSU or 2S LiPo + 6 V UBEC; BLE transport (same protocol); Android app.

## Left to the builder

Task breakdown and order; wiring-diagram tooling; Uno HC-05 serial choice; firmware file structure; exact UI layout details within the sketch; test approach; default speeds and initial home pose values (to be calibrated on hardware).
