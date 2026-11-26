# Sixth servo (wrist1, joint 6) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the sixth MG90 (wrist1, joint 6) to the ESP32 and Uno builds, keep the ESP8266 build at five joints, and bring firmware, protocol, tests and wiring docs in line with the six-servo arm.

**Architecture:** The joint count becomes a build setting: `kJointCount` is taken from `ROBOARM_JOINT_COUNT` (default 6). The ESP8266 sketch sets 5 before any include. The protocol moves to version 2: `A` and `STATE` carry `kJointCount` values, and `INFO` reports `joints=N`. Core code and tests stay board-independent and are built twice, for 6 and 5 joints.

**Tech Stack:** Arduino C++ (ESP32Servo, Servo), native g++ tests with the in-repo `check.h` framework, arduino-cli via the Makefile, SVG wiring diagrams.

**Spec:** [docs/superpowers/specs/2026-11-27-wrist-sixth-servo-design.md](../specs/2026-11-27-wrist-sixth-servo-design.md)

## Global Constraints

- Joint numbers: 1 claw (MG90), 2 wrist0 (MG90), 3 elbow (MG996), 4 shoulder (MG996), 5 base (MG996), **6 wrist1 (MG90, new)**. Joints 1–5 keep their pins on each board.
- ESP32 joint 6: **GPIO 33**, header label TBD (owner verifies on silkscreen). Uno joint 6: **D11**. ESP8266: no joint 6, stays 5 joints.
- Home order, six joints: `{4, 3, 2, 5, 1, 0}` (base, shoulder, elbow, wrist1, wrist0, claw). Five joints (ESP8266): `{4, 3, 2, 1, 0}`.
- Joint 6 is **not locked** before calibration; limits start at min 30, max 150, home 90.
- `kProtocolVersion` = `2`; `kFirmwareVersion` = `"0.3.0"`.
- Wiring: ESP32 and Uno go to **wiring v2**; ESP8266 stays **wiring v1**.
- Servo PSU: max 6.0 V, first tests 5.5 V, current limit 3 A; PSU + never to board 5V/VIN; PSU − to board GND.
- No commits: repo rule is commit only when asked. Leave changes in the working tree.

## Review Focus

1. Old app sends a 5-value `A` to the 6-joint firmware → `ERR bad command`, no servo writes. (Task 1)
2. `A` with more values than joints → `ERR bad command`, no servo writes. (Task 1)
3. `M 6` on the ESP8266 (5-joint) build → `ERR bad joint`. (Task 1, run in the 5-joint test build)
4. Joint 6 commanded outside 30–150 → `WARN 6 clamped to <limit>` and the clamped value is written. (Task 1)
5. Homing on the 6-joint build attaches joint 6 after elbow and before wrist0, and before claw. (Task 1)

## Files Overview

- `roboarm/src/core/Config.h` — `kJointCount` from `ROBOARM_JOINT_COUNT` (default 6). (Task 1)
- `roboarm/src/core/Version.h` — protocol 2, firmware 0.3.0. (Task 1)
- `roboarm/src/core/Arm.cpp` — `A`, `STATE`, `INFO` replies sized by `kJointCount`. (Task 1)
- `Makefile` — native `test` target builds and runs both joint counts. (Task 1)
- `test/test_arm.cpp`, `test/test_protocol.cpp` — count-independent tests. (Task 1)
- `roboarm/src/boards/config_esp32.h`, `config_uno.h`, `config_esp8266.h` — pins, home order, wiring version. (Task 2)
- `roboarm/roboarm.ino` — ESP8266 define before includes. (Task 2)
- `docs/wiring/esp32-d1r32.svg`, `docs/wiring/uno-hc05.svg`, `docs/wiring/README.md`, `docs/wiring/CHANGELOG.md`, `README.md` — diagrams, tables, checklist, changelog. (Task 3)
- `docs/calibration.md` — joint 6 in the calibration procedure. (Task 4)

---

### Task 1: Joint count per build and protocol v2 (core)

**Files:**
- Modify: `roboarm/src/core/Config.h`, `roboarm/src/core/Version.h`, `roboarm/src/core/Arm.cpp`, `Makefile:24-27`
- Test: `test/test_arm.cpp`, `test/test_protocol.cpp`

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces:
  - `constexpr uint8_t kJointCount` (in `Config.h`), value `ROBOARM_JOINT_COUNT`, default 6.
  - `kProtocolVersion == 2`, `kFirmwareVersion == "0.3.0"`.
  - Reply formats (unchanged field order, N = `kJointCount`):
    - `OK A d1 ... dN`
    - `STATE d1 ... dN attached speed`
    - `INFO RoboArm fw=0.3.0 proto=2 board=<name> wiring=<w> joints=N`
  - `parseCommand` already rejects wrong value counts for `A` (no change in `Protocol.cpp`).

- [ ] **Step 1: Write the failing tests**

In `test/test_arm.cpp`, make `makeCfg()` use a homeOrder that matches the build:

```cpp
#if kJointCount == 6
  const uint8_t order[kJointCount] = {4, 3, 2, 5, 1, 0};
#else
  const uint8_t order[kJointCount] = {4, 3, 2, 1, 0};
#endif
```

Add these tests (names and assertions; build the expected strings with `snprintf` from `kJointCount`):

```cpp
TEST(info_reports_proto2_and_joints) {
  // expect "INFO RoboArm fw=0.3.0 proto=2 board=testboard wiring=7 joints=<kJointCount>"
}
TEST(query_reports_every_joint) {
  // after arm(a): "?" reply has kJointCount positions, then attached=1 and speed=60
}
TEST(all_with_one_value_too_few_is_rejected_without_motion) {
  // build "A" followed by kJointCount-1 values; reply == "ERR bad command"
  // FakeServoOut.count('w') unchanged
}
TEST(all_with_one_value_too_many_is_rejected_without_motion) {
  // build "A" followed by kJointCount+1 values; reply == "ERR bad command"
}
TEST(homing_attaches_in_home_order) {
  // after cmd "H" and run(a, 2000): the 'a' calls, in order, equal cfg.homeOrder[0..kJointCount-1]
}
TEST(move_clamps_last_joint) {
  // arm(a); "M <kJointCount> 200" → reply "WARN <kJointCount> clamped to 150" is first line, last write == 150
}
```

In `test/test_protocol.cpp`:

```cpp
TEST(parse_bad_joint) {
  // existing "M 0 90" → BadJoint, plus "M <kJointCount+1> 90" → BadJoint
}
TEST(parse_all_needs_exact_count) {
  // "A" + (kJointCount-1) values → BadCommand
  // "A" + kJointCount values → None
  // "A" + (kJointCount+1) values → BadCommand
}
```

Also update the existing hard-coded five-value expectations in `test_arm.cpp` and `test_protocol.cpp` (grep for `OK A`, `STATE`, `proto=1`, `M 6`) so they use `kJointCount`.

- [ ] **Step 2: Run tests to verify they fail**

Run: `make test` (the native target at `Makefile:24`; with `LOCAL=1` or via Docker if the native target is not reached directly)
Expected: FAIL lines for the new tests (`proto=2`, `joints=`, `0.3.0` missing; `joints` not reported).

- [ ] **Step 3: Implement**

- `Config.h`: replace `constexpr uint8_t kJointCount = 5;` with:
  ```cpp
  #ifndef ROBOARM_JOINT_COUNT
  #define ROBOARM_JOINT_COUNT 6
  #endif
  constexpr uint8_t kJointCount = ROBOARM_JOINT_COUNT;  // joint N <-> index N-1
  ```
- `Version.h`: `kFirmwareVersion = "0.3.0"`, `kProtocolVersion = 2`.
- `Arm.cpp`:
  - Raise the `buf` size in `handleLine` to 96.
  - `CmdType::All` reply: loop over `kJointCount` and append `%d` values into `buf`.
  - `CmdType::Query` reply: loop the positions, then `attached` and `speed` as now.
  - `CmdType::Info`: raise `info` to 96 and add ` joints=%u` with `kJointCount`.
- No code change in `Protocol.cpp`: its loops already use `kJointCount`.

- [ ] **Step 4: Make the native test target run both joint counts**

In `Makefile`, after the existing `g++ ... -o build/tests` and `./build/tests` lines (tab-indented), add:

```make
	g++ -std=c++17 -Wall -Wextra -Werror -DROBOARM_JOINT_COUNT=5 -Iroboarm/src $(CORE_SRCS) $(wildcard test/*.cpp) -o build/tests5
	./build/tests5
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `make test`
Expected: both `build/tests` (6 joints) and `build/tests5` (5 joints) print `N passed, 0 failed`. The 5-joint run includes the `M 6` → `ERR bad joint` case (Review Focus 3).

---

### Task 2: Board configs and the ESP8266 define

**Files:**
- Modify: `roboarm/src/boards/config_esp32.h`, `roboarm/src/boards/config_uno.h`, `roboarm/src/boards/config_esp8266.h`, `roboarm/roboarm.ino` (top of file)

**Interfaces:**
- Consumes: `kJointCount` / `ROBOARM_JOINT_COUNT` from Task 1 (`Config.h`).
- Produces: `kConfig` for each board with `kJointCount` entries in `joints`, `homeOrder`; `wiringVersion`.

- [ ] **Step 1: Set the ESP32 config**

In `config_esp32.h`, add a sixth entry `{33, 30, 150, 90}` at index 5 of `joints`. Set `homeOrder` to `{4, 3, 2, 5, 1, 0}` with the comment `// homeOrder: base, shoulder, elbow, wrist1, wrist0, claw`. Set `wiringVersion` to `2`. Update the header comment to list six joints.

- [ ] **Step 2: Set the Uno config**

In `config_uno.h`, add `{11, 30, 150, 90}` at index 5. Set `homeOrder` to `{4, 3, 2, 5, 1, 0}`. Set `wiringVersion` to `2`.

- [ ] **Step 3: Keep the ESP8266 config at five joints**

In `config_esp8266.h`, leave the five entries and `wiringVersion` (`1`) as they are. Set `homeOrder` to `{4, 3, 2, 1, 0}` (already so). Update the comment to say five joints, no joint 6.

- [ ] **Step 4: Set the ESP8266 joint count in the sketch**

In `roboarm/roboarm.ino`, above the first `#include`, add:

```cpp
#if defined(ESP8266)
#define ROBOARM_JOINT_COUNT 5  // no safe servo GPIO left for joint 6
#endif
```

It must come before any include, because `Config.h` reads the macro when `Arm.h` is included.

- [ ] **Step 5: Verify all three boards compile**

Run: `make build-esp32 build-uno build-esp8266` (add `LOCAL=1` if arduino-cli runs natively; see `docs/flashing.md`)
Expected: each target compiles without error. If arduino-cli or Docker is unavailable here, say so in the final report; do not claim a pass.

---

### Task 3: Wiring diagrams, pin tables, checklist, changelog

**Files:**
- Modify: `docs/wiring/esp32-d1r32.svg`, `docs/wiring/uno-hc05.svg`, `docs/wiring/README.md`, `docs/wiring/CHANGELOG.md`, `README.md:3`
- Do not change: `docs/wiring/esp8266-d1.svg` (stays wiring v1, five joints).

**Interfaces:**
- Consumes: pins and wiring version from Task 2.
- Produces: wiring v2 diagrams and tables for ESP32 and Uno.

- [ ] **Step 1: Find the title blocks**

Run: `grep -n 'Wiring v' docs/wiring/*.svg`
Expected: the title block format from `docs/wiring/CHANGELOG.md` (`Wiring v<N> · <board> · Stage <S> · <date> · firmware config: config_<board>.h`). If a diagram lacks it, add it per `docs/wiring/README.md`.

- [ ] **Step 2: Edit `esp32-d1r32.svg`**

- Title block → `Wiring v2`.
- Rename the J2 box label `J2 Wrist` → `J2 Wrist0`.
- Add a servo box labelled `J6 Wrist1 (MG90)` between the J2 and J3 blocks, with `J6 signal` label, brown/red/orange wires to the rail as the others have, and signal to **GPIO33**. Header label: `TBD (verify silkscreen)`.
- Do not move existing J1–J5 signal lines.

- [ ] **Step 3: Edit `uno-hc05.svg`**

- Title block → `Wiring v2`.
- Rename `J2 Wrist` → `J2 Wrist0`.
- Add `J6 Wrist1 (MG90)` with signal to **D11**.
- Update the pin text `Pins above: J1 D3, J2 D5, J3 D6, J4 D9, J5 D10 (left to right)` to include `J6 D11`.
- HC-05 wiring unchanged.

- [ ] **Step 4: Edit `docs/wiring/README.md`**

- Joints row in "Parts and limits": `J1 Claw (MG90), J2 Wrist0 (MG90), J6 Wrist1 (MG90), J3 Elbow (MG996), J4 Shoulder (MG996), J5 Base (MG996)`.
- ESP32 pin table: add `| J6 Wrist1 | MG90 | GPIO33 | TBD (verify on silkscreen) |`.
- Uno pin table: add `| J6 Wrist1 | MG90 | D11 |`.
- ESP8266 section: add one sentence: "Five joints only; the board has no safe servo GPIO for a sixth."
- Checklist step 5: "J1 to J5" → "J1 to J6".
- Checklist step 10: homing order → "base, shoulder, elbow, wrist1, wrist0, claw".
- Add a checklist step after the current step 3 (rail wiring): "The sixth servo (wrist1) is on the same servo rail and capacitor as the others: red to +, brown to −." Power numbers stay as they are (6.0 V max, 3 A first limit). Do not add a new current figure; the owner's power budget is an open item.

- [ ] **Step 5: Edit `docs/wiring/CHANGELOG.md`**

Under `## esp32-d1r32`, add:
`- **v2** (2026-11-27, Stage 1): sixth servo wrist1 (J6) on GPIO33. Min firmware 0.3.0.`
Under `## uno`, add:
`- **v2** (2026-11-27, Stage 1): sixth servo wrist1 (J6) on D11. Min firmware 0.3.0.`
No entry for `esp8266-d1` (wiring unchanged).

- [ ] **Step 6: Edit root `README.md:3`**

Change "5-joint servo robot arm (Claw, Wrist, Elbow, Shoulder, Base)" to "6-joint servo robot arm on ESP32 and Uno (Claw, Wrist0, Wrist1, Elbow, Shoulder, Base); 5-joint on ESP8266".

- [ ] **Step 7: Verify no stale five-servo text remains for ESP32/Uno**

Run: `grep -rn -E 'J1 to J5|five|5-joint|J1 D3, J2 D5, J3 D6, J4 D9, J5 D10' README.md docs/wiring`
Expected: matches only in the ESP8266 section or as "5-joint" for ESP8266. Anything else is a missed edit.

---

### Task 4: Calibration procedure

**Files:**
- Modify: `docs/calibration.md`

**Interfaces:**
- Consumes: joint numbering from Global Constraints.

- [ ] **Step 1: Read the file**

Read `docs/calibration.md` in full before editing.

- [ ] **Step 2: Add joint 6**

Add J6 (wrist1, MG90, ESP32 GPIO33 / Uno D11) to every per-joint list in the procedure. State that the initial 30/150/90 values are starting points, not calibrated values. Keep the one-servo-at-a-time rule. Add J6 to the step that lists the claw/J1 first-power-up order only if the file has such a list.

- [ ] **Step 3: Verify**

Run: `grep -n 'J6\|wrist1' docs/calibration.md`
Expected: J6 appears in every per-joint list in the file.

---

### Task 5: Full verification

**Files:** none modified.

- [ ] **Step 1: Native tests, both joint counts**

Run: `make test`
Expected: `build/tests` and `build/tests5` each print `N passed, 0 failed`; the Python unittest discovery passes.

- [ ] **Step 2: Compile all three boards**

Run: `make build-esp32 build-uno build-esp8266`
Expected: three successful compiles. If unavailable, state which were not run.

- [ ] **Step 3: Protocol strings**

Run the INFO check from the 6-joint test build (`build/tests`). Expected string contains `proto=2`, `fw=0.3.0`, `joints=6`.

- [ ] **Step 4: Review the diff**

Run: `git status` and `git diff --stat`.
Expected: only the files listed in Files Overview, plus the two new docs (spec and this plan). No commit.

- [ ] **Step 5: Report**

Report: tests and compiles run and their results; the open items (companion app `roboarm-control` update for protocol 2; power budget for six servos; GPIO33/D11 header labels). State that nothing is committed.
