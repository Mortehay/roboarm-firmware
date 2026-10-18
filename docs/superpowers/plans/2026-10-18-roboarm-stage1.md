# RoboArm Stage 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Control the 5-servo arm from a Linux laptop over Bluetooth Classic, with safe wiring docs, tested firmware for ESP32 D1 R32 (and Uno + HC-05), and a Tkinter control app, in two public GitHub repos tracked in Plane.

**Architecture:** Firmware keeps all logic (line parsing, motion interpolation, arming/homing state machine) in Arduino-free C++ under `roboarm/src/core/`, unit-tested natively with g++; thin board glue in the `.ino` wires it to servos and serial/Bluetooth. The control app keeps protocol, link, controller and pose storage in plain Python modules tested with pytest against a simulated arm (`FakeArm`); the Tkinter GUI is a thin layer over `ArmController`. The line-based text protocol is the only contract between the repos.

**Tech Stack:** Arduino C++ (arduino-cli, `esp32:esp32` core 3.3.7, `arduino:avr`, ESP32Servo, Servo, BluetoothSerial), g++ (native tests), Python 3.12, pyserial ≥3.5, Tkinter, pytest, gh CLI, Plane MCP.

**Spec:** `docs/superpowers/specs/2026-10-18-roboarm-design.md` (in this repo)

## Global Constraints

- Firmware repo: `/home/markunn/Arduino/RoboArm` → GitHub `Mortehay/roboarm-firmware`, **public**, branch `main`.
- Control repo: `/home/markunn/Arduino/RoboArmControl` → GitHub `Mortehay/roboarm-control`, **public**, branch `main`.
- `.mcp.json` (contains Plane API key) must be gitignored in **both** repos and never committed; commit `.mcp.json.example` with the key replaced by `<PLANE_API_KEY>`.
- Joint numbering (protocol and UI): 1 Claw (MG90), 2 Wrist (MG90), 3 Elbow (MG996), 4 Shoulder (MG996), 5 Base (MG996).
- Servo supply: max **6.0 V**; first tests 5.5 V, current limit 3 A. PSU never connects to board 5V/VIN; PSU − tied to board GND.
- ESP32 servo pins: J1 GPIO13, J2 GPIO14, J3 GPIO27, J4 GPIO25, J5 GPIO26. Never use GPIO 0, 2, 12, 15, 34, 35, 36, 39.
- Uno servo pins: J1 D3, J2 D5, J3 D6, J4 D9, J5 D10. HC-05 on hardware Serial (D0/D1) — chosen to avoid SoftwareSerial servo jitter; HC-05 must be unplugged from D0/D1 while flashing; HC-05 RX via 1 kΩ/2 kΩ divider.
- Initial limits: every joint min 30°, max 150°, home 90° (until calibrated in Task B7).
- Serial: 115200 baud, `\n`-terminated ASCII lines (accept and strip `\r`), max line length 48 chars. Bluetooth device name `RoboArm`.
- Servos detached at boot and after `R`; only `H` attaches. Disconnect = hold position.
- Speeds (deg/s): default 60, homing 30, valid range 5–180; app presets slow 30 / med 60 / fast 120.
- Firmware core must compile on AVR: no STL, no exceptions, no heap allocation, fixed buffers.

## Review Focus

1. **Garbage on the serial line** (partial lines, CRLF, over-length, non-numbers) → firmware replies `ERR ...` and keeps working; buffer recovers on next `\n`. Tests in B2.
2. **Board resets on USB port open / reboots mid-session** → arm comes up detached, app reads `STATE ... 0 ...`, disables sliders until Home. Tests in B4 (boot state) and C4 (connect flow).
3. **Slider dragged fast** → app sends at most one `M` per joint per 50 ms, last value wins; never floods the link. Test in C4.
4. **Commands during homing or while detached** (user drags slider) → firmware `ERR homing` / `ERR not armed`; app resyncs with `?` instead of showing a fake angle. Tests in B4 and C4.
5. **Bluetooth drops mid-move** → firmware holds position (no detach); app marks disconnected, disables controls, does not crash. Tests in B4 (no detach without `R`) and C3/C4.

---

## Part A — Repos and tracking

### Task A1: Publish both repos to GitHub

**Files:**
- Modify: `RoboArm/.gitignore` (already contains `.mcp.json`)
- Create: `RoboArm/.mcp.json.example`, `RoboArm/README.md`
- Create: `RoboArmControl/.mcp.json.example`, `RoboArmControl/README.md` (stub, filled in C6)
- Modify: `RoboArmControl/.gitignore` (append `.mcp.json`, `.venv/`, `__pycache__/`, `*.egg-info/`, `.pytest_cache/` — ensure the existing last line keeps its own newline)

- [ ] **Step 1:** In RoboArm: `git branch -m master main`. Create `.mcp.json.example` (copy of `.mcp.json` with the bearer token replaced by `<PLANE_API_KEY>`). README: one paragraph + links to spec, plan, `docs/wiring/`, `docs/flashing.md`.
  Note: each repo's `.env` already holds the *other* repo's path (`RoboArm/.env` → `CONTROL_APP_PATH`, `RoboArmControl/.env` → `ROBOARM_APP_PATH`); only add the missing trailing newline.
- [ ] **Step 2:** Verify no secret is tracked: `git ls-files | grep -c mcp.json$` → `0`; `git grep -n plane_api_` → no output.
- [ ] **Step 3:** Commit, then `gh repo create Mortehay/roboarm-firmware --public --source . --remote origin --push`. Expected: URL printed, `git status -sb` shows `## main...origin/main`.
- [ ] **Step 4:** In RoboArmControl: `git init -b main`, update `.gitignore`, add `.mcp.json.example` and stub README, run the Step 2 checks there, commit, `gh repo create Mortehay/roboarm-control --public --source . --remote origin --push`.

### Task A2: Plane project, modules, work items

Requires the Plane MCP server (`.mcp.json`) loaded in the session; done by the orchestrating session, not a subagent. Follow the `plane-workflow` skill.

- [ ] **Step 1:** In workspace `roboarm`, find or create project **RoboArm**.
- [ ] **Step 2:** Create modules **Control App**, **Firmware & Flashing**, **Stage 2**.
- [ ] **Step 3:** Create one work item per task in this plan: A1 and B1–B7 → Firmware & Flashing; C1–C6 → Control App. Title = task title, description = link to this plan section + its deliverable. Mark A1 done if already complete.
- [ ] **Step 4:** Stage 2 backlog items: "PCA9685 servo driver board", "Dedicated 6 V ≥10 A PSU (or 2S LiPo + 6 V UBEC)", "BLE transport (same protocol)", "Android control app".
- [ ] **Step 5:** Verify: list items per module; counts 8 / 6 / 4.

---

## Part B — Firmware (`/home/markunn/Arduino/RoboArm`)

File layout (locked):

```
roboarm/roboarm.ino            board glue (ESP32 vs Uno via #if defined(ESP32))
roboarm/src/core/Config.h      JointConfig, ArmConfig, kJointCount
roboarm/src/core/LineReader.h/.cpp
roboarm/src/core/Protocol.h/.cpp
roboarm/src/core/Motion.h/.cpp
roboarm/src/core/Arm.h/.cpp
roboarm/src/boards/config_esp32.h, config_uno.h   (const ArmConfig kConfig)
test/check.h                   minimal test macros (CHECK, CHECK_EQ, TEST registry)
test/test_*.cpp, test/main.cpp
scripts/setup-arduino-cli.sh
Makefile
docs/wiring/README.md, esp32-d1r32.svg, uno-hc05.svg
docs/flashing.md, docs/calibration.md
```

Core files include only `<stdint.h>`, `<stddef.h>`, `<string.h>`, `<stdlib.h>`, `<stdio.h>` (snprintf) — no `Arduino.h`.

### Task B1: Toolchain, build skeleton, native test harness

**Files:** Create `scripts/setup-arduino-cli.sh`, `Makefile`, `roboarm/roboarm.ino` (empty `setup`/`loop` that prints `READY` on Serial at 115200), `test/check.h`, `test/main.cpp`, `test/test_smoke.cpp`.

**Interfaces — Produces:** Make targets `test`, `build-esp32`, `build-uno`, `flash-esp32 PORT=`, `flash-uno PORT=`, `monitor PORT=` (defaults `PORT=/dev/ttyUSB0`). `test/check.h` provides `TEST(name)`, `CHECK(expr)`, `CHECK_EQ(a,b)`, `CHECK_STR(a,b)`; runner prints `N passed, M failed` and exits non-zero on failure.

- [ ] **Step 1:** `setup-arduino-cli.sh`: install arduino-cli to `~/.local/bin` via the official install script if missing; `core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json`; `core install esp32:esp32 arduino:avr` (same URL flag); `lib install ESP32Servo Servo`. Idempotent; `set -euo pipefail`.
- [ ] **Step 2:** Run it. Expected: `arduino-cli core list` shows `esp32:esp32` and `arduino:avr`; `arduino-cli lib list` shows ESP32Servo and Servo.
- [ ] **Step 3:** Makefile: `test` = `g++ -std=c++17 -Wall -Wextra -Werror -Iroboarm/src roboarm/src/core/*.cpp test/*.cpp -o build/tests && ./build/tests`; `build-esp32` = `arduino-cli compile --fqbn esp32:esp32:esp32 roboarm`; `build-uno` with `arduino:avr:uno`; flash targets = compile + `upload -p $(PORT) --fqbn ...`; `monitor` = `arduino-cli monitor -p $(PORT) -c baudrate=115200`.
- [ ] **Step 4:** `test_smoke.cpp` with `TEST(smoke) { CHECK_EQ(1 + 1, 2); }`. Run `make test` → `1 passed, 0 failed`.
- [ ] **Step 5:** `make build-esp32` and `make build-uno` both succeed.
- [ ] **Step 6:** Add `build/` to `.gitignore`. Commit `build: arduino-cli setup, Makefile, native test harness`.

### Task B2: Line reader and command parser

**Files:** Create `roboarm/src/core/Config.h`, `LineReader.h/.cpp`, `Protocol.h/.cpp`; Test `test/test_protocol.cpp`.

**Interfaces — Produces:**
```cpp
// Config.h
constexpr uint8_t kJointCount = 5;   // joint number N ↔ index N-1
struct JointConfig { uint8_t pin; int16_t minDeg, maxDeg, homeDeg; };
struct ArmConfig { JointConfig joints[kJointCount]; uint8_t homeOrder[kJointCount]; // indices
                   float defaultSpeed, homingSpeed; uint16_t settleMs; };
// LineReader.h
class LineReader { public:
  static constexpr size_t kMaxLen = 48;
  enum Result : uint8_t { kNone, kLine, kOverflow };
  Result feed(char c);              // '\r' ignored; '\n' completes; >kMaxLen chars → kOverflow once at '\n', then reset
  const char* line() const; };       // valid after kLine until next feed
// Protocol.h
enum class CmdType : uint8_t { Move, All, Speed, Home, Stop, Release, Query };
struct Command { CmdType type; uint8_t joint; int16_t values[kJointCount]; };
enum class ParseError : uint8_t { None, BadCommand, BadJoint };
ParseError parseCommand(const char* line, Command& out);
```

- [ ] **Step 1: Write failing tests:**
  - `line_basic`: feeding `"M 3 110\n"` → last result `kLine`, `line()` == `"M 3 110"`.
  - `line_crlf`: `"?\r\n"` → `"?"`.
  - `line_overflow_recovers`: 60×`'x'` then `'\n'` → `kOverflow`; then `"H\n"` → `kLine` `"H"`.
  - `parse_move`: `"M 3 110"` → None, Move, joint 3, values[0] 110.
  - `parse_all`: `"A 10 20 30 40 50"` → All, values {10,20,30,40,50}.
  - `parse_simple`: `"H"`,`"S"`,`"R"`,`"?"` → Home/Stop/Release/Query; `"V 60"` → Speed values[0] 60.
  - `parse_bad_joint`: `"M 0 90"`, `"M 6 90"` → BadJoint.
  - `parse_garbage`: `""`, `"X"`, `"M"`, `"M 3"`, `"M 3 abc"`, `"A 1 2 3"`, `"M 3 90 extra"`, `"V"` → BadCommand.
  - Extra spaces between tokens (`"M  3   110"`) accepted.
- [ ] **Step 2:** `make test` → fails (missing headers).
- [ ] **Step 3:** Implement. Parse with `strtol` and end-pointer checks; reject trailing non-space characters.
- [ ] **Step 4:** `make test` → all pass.
- [ ] **Step 5:** Commit `feat(core): line reader and command parser`.

### Task B3: Speed-limited motion

**Files:** Create `roboarm/src/core/Motion.h/.cpp`; Test `test/test_motion.cpp`.

**Interfaces — Produces:**
```cpp
class Motion { public:
  void reset(uint8_t j, float deg);            // position = target = deg (no motion)
  void setTarget(uint8_t j, float deg);
  void setSpeed(float degPerSec);
  float speed() const;
  void holdAll();                              // target = current position, all joints
  void update(uint32_t nowMs);                 // first call only records time
  float position(uint8_t j) const;
  float target(uint8_t j) const;
  bool atTarget(uint8_t j) const; };           // |pos-target| < 0.5
```

- [ ] **Step 1: Failing tests:**
  - `motion_moves_at_speed`: reset(0,90), speed 60, target 150; update(0), update(500) → position 120 (±0.01); update(1500) → 150 and atTarget.
  - `motion_no_overshoot`: target 91 from 90, update(0), update(1000) → exactly 91.
  - `motion_downward`: from 90 to 30 at 60 deg/s, after 500 ms → 60.
  - `motion_hold`: mid-move `holdAll()` → target == position; further updates don't move.
  - `motion_millis_wrap`: update(0xFFFFFF00), update(0x00000100) (512 ms elapsed) moves 30.72° at 60 deg/s.
- [ ] **Step 2:** `make test` → fail.
- [ ] **Step 3:** Implement; elapsed = `uint32_t` subtraction.
- [ ] **Step 4:** `make test` → pass.
- [ ] **Step 5:** Commit `feat(core): speed-limited motion`.

### Task B4: Arm state machine (arming, homing, clamping, replies)

**Files:** Create `roboarm/src/core/Arm.h/.cpp`; Test `test/test_arm.cpp` (with `FakeServoOut` recording attach/detach/write calls and `VecSink` collecting reply lines).

**Interfaces:**
- Consumes: `ArmConfig`, `LineReader`, `parseCommand`, `Motion` (B2, B3).
- Produces:
```cpp
class ServoOut { public: virtual void attach(uint8_t j) = 0; virtual void detach(uint8_t j) = 0;
                         virtual void write(uint8_t j, int16_t deg) = 0; };
class ReplySink { public: virtual void line(const char* text) = 0; };
enum class ArmState : uint8_t { Detached, Homing, Armed };
class Arm { public:
  Arm(const ArmConfig& cfg, ServoOut& out);
  void handleLine(const char* line, ReplySink& reply);
  void update(uint32_t nowMs);
  ArmState state() const; };
```

Reply rules (exact text; every command ends with exactly one `OK…`/`ERR…`/`STATE…` line, `WARN` lines come first):
- `M j d` armed → `OK M j d'` (d' = clamped); if clamped, first `WARN j clamped to d'`.
- `A d1..d5` armed → WARN per clamped joint, then `OK A d1'..d5'`.
- `M`/`A` when Detached → `ERR not armed`; when Homing → `ERR homing`.
- `V s` (5–180) → `OK V s`; outside → `ERR bad speed`.
- `H` → `OK H`. From Detached: state Homing; for each index in `homeOrder`: `attach`, `write(home)`, `Motion::reset(j, home)`, wait `settleMs`, next; then Armed with speed restored. From Armed: state Homing, targets = home at `homingSpeed`, joints moved one at a time in `homeOrder` (next starts when previous `atTarget`), then Armed. From Homing: `OK H` (no restart).
- `S` → `OK S`. Armed: `Motion::holdAll()`, stays attached. Homing: abort homing, detach all, state Detached (owner must `H` again). Detached: no-op.
- `R` → detach all, `OK R`, state Detached.
- `?` → `STATE p1 p2 p3 p4 p5 a v` (positions rounded to int, a = 1 only when Armed, v = speed as int).
- Parse errors → `ERR bad command` / `ERR bad joint`.
- `update()` writes `round(position)` to each attached joint only when the rounded value changed.

- [ ] **Step 1: Failing tests** (config: limits 30–150, home 90, homeOrder {4,3,2,1,0}, defaultSpeed 60, homingSpeed 30, settleMs 300):
  - `boot_detached`: new Arm → state Detached, no attach calls; `?` → `"STATE 90 90 90 90 90 0 60"`.
  - `move_rejected_when_detached`: `M 3 100` → `"ERR not armed"`, no write.
  - `home_staggered`: `H` → `"OK H"`; update(0) → only joint index 4 attached & written 90; update(299) → still one; update(300) → index 3 attached; … after 5×300 ms state Armed.
  - `move_rejected_while_homing`: during homing `M 1 90` → `"ERR homing"`.
  - `move_and_clamp`: armed, `M 3 170` → lines `["WARN 3 clamped to 150", "OK M 3 150"]`; after enough updates last write for index 2 is 150.
  - `move_all`: `A 10 90 90 90 90` → `["WARN 1 clamped to 30", "OK A 30 90 90 90 90"]`.
  - `stop_holds`: mid-move `S` → `"OK S"`; positions stop changing over further updates; servos stay attached.
  - `release_detaches`: `R` → `"OK R"`, 5 detach calls, state Detached, `?` armed flag 0.
  - `no_detach_without_R`: 10 s of `update()` with no input after armed → zero detach calls (Review Focus 5).
  - `speed`: `V 4` → `"ERR bad speed"`; `V 120` → `"OK V 120"`, `?` ends with `120`.
  - `bad_lines`: `"zzz"` → `"ERR bad command"`; `"M 9 90"` → `"ERR bad joint"`.
- [ ] **Step 2:** `make test` → fail.
- [ ] **Step 3:** Implement; format with `snprintf` into a 48-byte buffer.
- [ ] **Step 4:** `make test` → pass.
- [ ] **Step 5:** Commit `feat(core): arm state machine with arming and staggered homing`.

### Task B5: Board glue and configs (ESP32 + Uno)

**Files:** Create `roboarm/src/boards/config_esp32.h`, `config_uno.h`; Modify `roboarm/roboarm.ino`.

**Interfaces — Consumes:** `Arm`, `ServoOut`, `ReplySink`, `LineReader`, `ArmConfig` (B2–B4).

- [ ] **Step 1:** Configs: `const ArmConfig kConfig` with the pins from Global Constraints, limits 30/150/90, homeOrder `{4, 3, 2, 1, 0}` (base, shoulder, elbow, wrist, claw), speeds 60/30, settleMs 300.
- [ ] **Step 2:** `.ino`: `ServoOut` implementation over an array of 5 `Servo` objects (`attach(pin, 500, 2400)`); `ReplySink` that writes to the stream the command came from (`Print&`). ESP32: `BluetoothSerial SerialBT; SerialBT.begin("RoboArm")`, one `LineReader` each for `Serial` and `SerialBT`. Uno: only `Serial` (HC-05 shares D0/D1). `loop()`: drain both streams into readers → `arm.handleLine`, then `arm.update(millis())`. On boot print `READY RoboArm` on every stream.
- [ ] **Step 3:** `make build-esp32` and `make build-uno` succeed; note flash/RAM usage from output in the commit message (Uno RAM must be < 75%).
- [ ] **Step 4 (hardware, servo PSU OFF, servos unplugged):** `make flash-esp32 PORT=/dev/ttyUSB0`; `make monitor` → `READY RoboArm`; type `?` → `STATE 90 90 90 90 90 0 60`; `M 1 90` → `ERR not armed`.
- [ ] **Step 5 (hardware):** pair from laptop per C6 README draft (`bluetoothctl` pair/trust, `sudo rfcomm bind 0 <MAC> 1`), `screen /dev/rfcomm0 115200`, `?` → STATE line.
- [ ] **Step 6:** Commit `feat: ESP32 and Uno board glue`.

### Task B6: Wiring diagrams, safety checklist, flashing guide

**Files:** Create `docs/wiring/esp32-d1r32.svg`, `docs/wiring/uno-hc05.svg`, `docs/wiring/README.md`, `docs/flashing.md`.

- [ ] **Step 1:** SVGs (hand-written, render on GitHub): PSU (GA305H, "5.5–6.0 V, 3 A limit"), servo rail with 470–1000 µF capacitor (polarity marked), 5 servos with wire colours (brown −, red +, orange signal), board with signal pins labelled both GPIO and header label (ESP32: GPIO13/D9, GPIO14/D7, GPIO27/D6, GPIO25/D3, GPIO26/D2 — marked "verify on your board's silkscreen"), common-ground wire highlighted, laptop USB, explicit "✗ do NOT connect PSU + to 5V/VIN". Uno diagram adds HC-05 with 1 kΩ/2 kΩ divider on HC-05 RX and note "unplug HC-05 from D0/D1 while flashing".
- [ ] **Step 2:** README: both diagrams embedded, pin tables, and the pre-power checklist (in order): PSU output off → set 5.5 V and 3 A → wire rail, capacitor polarity check → common ground → no PSU wire to board 5V/VIN → board on USB, firmware shows `READY` → PSU output on (servos stay limp) → press Home → raise current limit toward 5 A only after motion verified. Plus "if the PSU hits current limit / board resets" troubleshooting.
- [ ] **Step 3:** `flashing.md`: run `scripts/setup-arduino-cli.sh`; find port (`ls /dev/ttyUSB* /dev/ttyACM*` before/after plugging in; `dmesg | tail`); brltty fix (`sudo apt remove brltty` if CH340 disconnects instantly); `make flash-esp32` / `make flash-uno`; "Failed to connect to ESP32" → hold BOOT during "Connecting…"; "port busy" → close monitor/app; servo PSU off while flashing.
- [ ] **Step 4:** Verify SVGs are well-formed: `python3 -c "import xml.dom.minidom,sys;[xml.dom.minidom.parse(f) for f in sys.argv[1:]]" docs/wiring/*.svg` → no output. Owner reviews diagrams visually before wiring.
- [ ] **Step 5:** Commit `docs: wiring diagrams, safety checklist, flashing guide`.

### Task B7: Hardware bring-up and calibration (with the owner)

**Files:** Create `docs/calibration.md`; Modify `roboarm/src/boards/config_esp32.h` (calibrated values).

- [ ] **Step 1:** Write `calibration.md`: one servo at a time, arm mechanically free; Home; jog with app (or `M j d` via monitor) in 5° steps toward each extreme, stop at first sign of strain/contact, back off 5°, record; choose home ≈ natural resting pose.
- [ ] **Step 2 (owner):** Follow checklist from B6, connect claw servo only, `H`, `M 1 60`, `M 1 120` → servo moves smoothly, PSU current stays < 1 A.
- [ ] **Step 3 (owner):** All five servos; calibrate each joint; enter values into `config_esp32.h`.
- [ ] **Step 4:** `make test && make build-esp32`, flash, verify `H` brings the arm to the new home one joint at a time.
- [ ] **Step 5:** Commit `chore: calibrated joint limits`; copy limits into control app `joints.py` (C1 interface) and commit there too.

---

## Part C — Control app (`/home/markunn/Arduino/RoboArmControl`)

File layout (locked):

```
pyproject.toml                       name roboarm-control, requires-python >=3.12, deps pyserial>=3.5, extra dev: pytest; script roboarm-control = roboarm_control.app:main
roboarm_control/__init__.py
roboarm_control/joints.py            joint table (mirrors firmware config)
roboarm_control/protocol.py          command builders + reply parser
roboarm_control/fake.py              FakeArm simulator implementing Link
roboarm_control/link.py              Link protocol, SerialLink, open_link, list_ports
roboarm_control/poses.py             PoseStore (JSON)
roboarm_control/controller.py        ArmController (throttle, jog, state, playback)
roboarm_control/app.py               Tkinter GUI
tests/test_protocol.py, test_fake.py, test_link.py, test_poses.py, test_controller.py
```

Environment: `sudo apt install python3-tk python3-venv` (owner), `python3 -m venv .venv && .venv/bin/pip install -e '.[dev]'`.

### Task C1: Package skeleton, joints, protocol

**Interfaces — Produces:**
```python
# joints.py
@dataclass(frozen=True)
class Joint: number: int; name: str; min_deg: int; max_deg: int; home_deg: int; key_up: str; key_down: str
JOINTS: tuple[Joint, ...]   # display order: 5 Base q/a, 4 Shoulder w/s, 3 Elbow e/d, 2 Wrist r/f, 1 Claw t/g; all 30/150/90
def joint(number: int) -> Joint
# protocol.py
def move(joint: int, deg: int) -> str          # "M 3 110"
def move_all(degs: Sequence[int]) -> str       # "A 10 20 30 40 50", degs ordered joint 1..5
def speed(deg_per_s: int) -> str               # "V 60"
HOME, STOP, RELEASE, QUERY = "H", "S", "R", "?"
@dataclass(frozen=True) class Ok: text: str
@dataclass(frozen=True) class Warn: joint: int | None; text: str
@dataclass(frozen=True) class Err: reason: str
@dataclass(frozen=True) class State: angles: tuple[int, int, int, int, int]; armed: bool; speed: int   # angles joint 1..5
@dataclass(frozen=True) class Unknown: text: str
Reply = Ok | Warn | Err | State | Unknown
def parse_reply(line: str) -> Reply
```

- [ ] **Step 1:** Create pyproject, venv, install.
- [ ] **Step 2: Failing tests** `tests/test_protocol.py`: `move(3,110)=="M 3 110"`; `move_all([30,90,90,90,90])=="A 30 90 90 90 90"`; `parse_reply("OK M 3 150")==Ok("M 3 150")`; `parse_reply("WARN 3 clamped to 150")==Warn(3,"3 clamped to 150")`; `parse_reply("ERR not armed")==Err("not armed")`; `parse_reply("STATE 90 91 92 93 94 1 60")==State((90,91,92,93,94),True,60)`; `parse_reply("READY RoboArm")==Unknown("READY RoboArm")`; malformed `"STATE 1 2"` → `Unknown`; `[j.number for j in JOINTS]==[5,4,3,2,1]`.
- [ ] **Step 3:** `.venv/bin/pytest -q` → fail. **Step 4:** implement. **Step 5:** pass.
- [ ] **Step 6:** Commit `feat: protocol and joint table`.

### Task C2: FakeArm simulator

**Interfaces — Produces** (also defines the `Link` protocol in `link.py`):
```python
class Link(Protocol):
    connected: bool
    def send(self, line: str) -> None: ...          # without '\n'
    def read_lines(self) -> list[str]: ...           # non-blocking, drains received lines
    def close(self) -> None: ...
class FakeArm:  # implements Link; mirrors firmware reply rules from plan Task B4, moves are instant, homing instant
    def __init__(self) -> None
```

- [ ] **Step 1: Failing tests** `tests/test_fake.py`: fresh → `?` gives `"STATE 90 90 90 90 90 0 60"`; `M 1 90` → `["ERR not armed"]`; after `H` → `["OK H"]`, then `?` armed 1; `M 3 170` → `["WARN 3 clamped to 150", "OK M 3 150"]`; `R` → armed 0; `V 4` → `["ERR bad speed"]`; `close()` → `connected` False and `send` raises `ConnectionError`.
- [ ] **Step 2–4:** fail → implement (reuse `joints.JOINTS` limits) → pass.
- [ ] **Step 5:** Commit `feat: FakeArm simulator`.

### Task C3: Serial link and port discovery

**Interfaces — Produces:**
```python
class SerialLink:  # implements Link
    def __init__(self, port: str, baud: int = 115200, serial_factory=serial.Serial)
def open_link(port: str) -> Link        # "fake" → FakeArm()
def list_ports() -> list[str]           # sorted /dev/rfcomm* + serial.tools.list_ports devices, then "fake"
```
`SerialLink` builds the port unopened, sets `dtr=False`, `rts=False`, then `open()` (avoids ESP32 auto-reset); a daemon reader thread splits on `\n`, strips `\r`, queues lines; any `SerialException`/`OSError` → `connected=False`, thread exits; `send` on a dead link raises `ConnectionError`.

- [ ] **Step 1: Failing tests** `tests/test_link.py` using a fake serial object (in-memory `read`/`write`, settable to raise): `send("?")` writes `b"?\n"`; incoming `b"OK H\r\nSTATE 90"` then `b" 90 90 90 90 1 60\n"` → `read_lines()` eventually `["OK H", "STATE 90 90 90 90 90 1 60"]`; read raising `SerialException` → `connected` becomes False within 1 s and `send` raises `ConnectionError`; dtr/rts set False before `open()` (record call order); `open_link("fake")` is a `FakeArm`.
- [ ] **Step 2–4:** fail → implement → pass.
- [ ] **Step 5:** Commit `feat: serial link with reset-safe open`.

### Task C4: Controller and pose store

**Interfaces — Produces:**
```python
class PoseStore:
    def __init__(self, path: Path)                      # default ~/.config/roboarm-control/poses.json
    def save(self, name: str, angles: Sequence[int]) -> None
    def get(self, name: str) -> list[int]
    def names(self) -> list[str]                        # insertion order
    # file format: {"poses": {"Home": [90, 90, 90, 90, 90]}}; missing/corrupt file → empty store, not crash
class ArmController:
    def __init__(self, link: Link, clock: Callable[[], float] = time.monotonic)
    angles: dict[int, int]; armed: bool; connected: bool; speed: int; log: deque[str]  # maxlen 200, "> cmd" / "< reply"
    def on_connect(self) -> None                         # sends "?"
    def request_move(self, joint: int, deg: int) -> None # pending, last wins
    def jog(self, joint: int, direction: int) -> None    # ±2°, clamped to joint limits, via request_move
    def home(self) -> None; def stop(self) -> None; def release(self) -> None
    def set_speed(self, deg_per_s: int) -> None
    def play(self, poses: Sequence[Sequence[int]]) -> None
    def tick(self) -> None   # GUI calls every 20 ms
```
Rules: `tick` sends a pending `M` for a joint only if ≥ 50 ms since that joint's last `M`; moves are dropped (not sent) while `armed` is False. Replies update state: `State` → angles/armed/speed; `Err` with reason `homing` or `not armed` → send `?`; `OK H` → poll `?` every 200 ms until armed. `stop()` sends `S` immediately (bypasses throttle) and clears pending moves and playback. Playback: send `A` for pose k, poll `?` every 200 ms until all angles within 1° of pose k (after clamping), then pose k+1. Link failure (`connected` False or `ConnectionError`) → `connected=False`, `armed=False`, pending cleared, no exception escapes `tick`.

- [ ] **Step 1: Failing tests** `tests/test_poses.py`: save/get/names round-trip through a file in `tmp_path`; corrupt JSON → `names()==[]`.
- [ ] **Step 2: Failing tests** `tests/test_controller.py` (FakeArm + fake clock):
  - `connect_reads_state`: `on_connect(); tick()` → `armed is False`, angles all 90.
  - `moves_blocked_until_armed`: request_move(3,100), tick → no `M` sent.
  - `throttle_last_wins`: armed; request_move(3, 100..109) ×10, tick → exactly one line `M 3 109`; then request_move(3, 120) at +10 ms, tick → nothing sent; at +50 ms tick → `M 3 120` (Review Focus 3).
  - `err_resyncs`: link replies `ERR homing` → next sent line is `?`.
  - `stop_bypasses_throttle`: stop() → `S` sent immediately, pending cleared.
  - `playback_sequence`: play([[90]*5, [60,90,90,90,90]]) → `A` lines sent in order, second only after state matches first.
  - `link_drop`: FakeArm closed mid-session → tick doesn't raise; `connected False`, `armed False` (Review Focus 5).
  - `reboot_detected`: armed, then link yields `READY RoboArm` and `STATE … 0 60` → `armed False` (Review Focus 2).
- [ ] **Step 3–5:** fail → implement → pass.
- [ ] **Step 6:** Commit `feat: arm controller with throttling, playback, pose store`.

### Task C5: Tkinter GUI

**Files:** Create `roboarm_control/app.py`.
**Interfaces — Consumes:** `ArmController`, `PoseStore`, `open_link`, `list_ports`, `JOINTS`.

Layout = approved sketch in spec: port combobox (from `list_ports`, editable) + Connect/Disconnect + status dot/label; one row per joint in `JOINTS` order with `Scale` (from `min_deg` to `max_deg`), angle label, key hints; speed radio buttons (30/60/120); buttons Home, STOP (red), Release, Save pose (asks name via `simpledialog`), Play (plays selected poses in listbox order); pose listbox; scrolling log (last 200 lines).
Behaviour: `root.after(20, tick)` loop; sliders and move keys disabled unless `connected and armed`; slider positions follow `controller.angles` except while the user is dragging; key press-and-hold → `jog` each tick while held (track `KeyPress`/`KeyRelease`); Space → STOP always active when connected.

- [ ] **Step 1:** Implement `main()`.
- [ ] **Step 2:** Manual check with port `fake`: connect → sliders disabled, status "connected, not armed"; Home → enabled; drag Elbow to 150 → log shows throttled `M 3 …` lines and `< OK M 3 150`; hold `e` → angle rises in 2° steps; Space → `> S`; Save pose "Pick", Play → `A` lines in log; Disconnect → controls disabled.
- [ ] **Step 3:** `.venv/bin/pytest -q` still passes. Commit `feat: Tkinter control GUI`.

### Task C6: README and end-to-end test on hardware

**Files:** Modify `README.md`.

- [ ] **Step 1:** README: install (`apt install python3-tk python3-venv`, venv, `pip install -e .`), run `roboarm-control`, fake mode, Bluetooth Classic pairing on Linux (`bluetoothctl` → `scan on`, `pair <MAC>`, `trust <MAC>`; `sudo rfcomm bind 0 <MAC> 1`; port `/dev/rfcomm0`; user in `dialout`), USB alternative `/dev/ttyUSB0`, link to firmware repo safety checklist, keyboard map.
- [ ] **Step 2 (owner, hardware):** Follow the firmware checklist; connect over `/dev/rfcomm0`; Home; move every joint; save and play a 3-pose sequence; STOP mid-move freezes; turn laptop Bluetooth off mid-move → arm holds, app shows disconnected; reconnect → state resyncs.
- [ ] **Step 3:** Commit `docs: setup, Bluetooth pairing, usage`; push both repos; close Plane items.
