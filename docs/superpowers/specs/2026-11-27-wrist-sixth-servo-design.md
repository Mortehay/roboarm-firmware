# Sixth servo (MG90, between J2 Wrist and J3 Elbow) — design

Date: 2026-11-27. Status: draft, awaiting owner approval.

## Intent and why

The physical arm has **six** servos. The wrist has two MG90s, and the second one sits between J2 Wrist and J3 Elbow. The firmware, the serial protocol, the board configs and the wiring diagrams all describe **five** servos. So the sixth servo can't be commanded, homed, calibrated or wired per the docs. This design makes it a first-class, independently controlled joint and brings the firmware, protocol and wiring docs in line with the hardware.

Owner decision (2026-11-27): the extra MG90 is a **separate joint**, not a second motor driving J2.

## Goals

- The sixth servo can be attached, homed, moved, stopped, released and calibrated like the other joints, on the ESP32 D1 R32 and the Arduino Uno.
- The wiring diagrams, pin tables, CHANGELOG and `wiringVersion` match the hardware.
- The existing five joints keep their numbers, pins and behaviour. Their existing commands still work.

## Non-goals

- Changing the desktop control app. It lives in the companion repo `roboarm-control`, which is not in this workspace. The protocol change in this design requires a matching app change; it is tracked separately (see Open items).
- Choosing the sixth joint's limits and home pose. They start as placeholders and are set by the owner through the existing calibration procedure (`docs/calibration.md`).
- Bluetooth LE, PCA9685 or a new PSU. Those remain Stage 2.

## Anti-goals (would be a failure even if it compiles)

- Any change to the five existing joints' pins, limits or home pose.
- A sixth servo that starts moving on boot or on power-up before `H`. The arming model (servos detached until `H`) still applies to all six joints.
- An old app or a mismatched firmware silently driving the wrong joint. A version mismatch must produce an error, not motion.
- Breaking the ESP8266 build or its wiring (see Constraints).

## Constraints

- **Pin budget.** ESP8266 has no spare safe servo GPIO: 13, 12, 14, 4 and 5 are in use, and 0, 2 and 15 are boot pins, 1 and 3 are USB serial, and 16 has no PWM. ESP8266 therefore cannot take a sixth servo as wired.
- **Power.** Six servos on one bench PSU. The rail rules in `docs/wiring/README.md` (6.0 V max, 3 A first limit, 470–1000 µF capacitor, PSU − to board GND, PSU + never to the board) are unchanged.
- **Wiring versioning.** Any pin change bumps the wiring version and updates the title block, `wiringVersion` in the board config, and `docs/wiring/CHANGELOG.md` (`docs/wiring/README.md`).
- **Protocol versioning.** `kProtocolVersion` is 1 today (`roboarm/src/core/Version.h`). The `A` command and the `STATE` reply change shape, so the version must change.
- **Current code is fixed at five.** `kJointCount = 5` in `roboarm/src/core/Config.h` is a single shared constant. `A` expects five values; `STATE` and `OK A` print five positions; `M` rejects joints above 5 (`roboarm/src/core/Protocol.cpp`, `roboarm/src/core/Arm.cpp`).

## Approaches considered

1. **Separate joint, joint count per board (chosen).** The sixth servo is joint 6. Joint count moves from the shared core constant into each board config. ESP32 and Uno use six; ESP8266 uses five. The protocol reports the count so the app can adapt.
2. **Paired with J2 (rejected by owner).** Both wrist servos take one angle on one joint number. No protocol change, but the two can't move independently. Rejected because the owner wants them independent.
3. **Separate joint, one fixed count of six for all boards (rejected).** Simpler code, but the ESP8266 build cannot be made to work (no pin), so it would be broken or need a fake sixth joint.

## Decisions

Owner decisions are marked **[owner]**. My calls are marked **[my call]** and can be overridden.

### Joint numbering and placement

- **[my call]** The new servo is **joint 6**, appended. Joints 1–5 keep their numbers. Numbering it 3 and shifting the others would break every existing command, app and doc. The physical position (between J2 and J3) is recorded in the label, not the number.
- **[owner]** Names, from the claw down: claw (J1, MG90), wrist0 (J2, MG90), **wrist1 (J6, MG90, the new servo, between wrist0 and elbow)**, elbow (J3, MG996), shoulder (J4, MG996), base (J5, MG996). The names are cosmetic; the numbers are what the protocol uses.
- Servo type: MG90, same as J1 and J2. Angle limits start at 30/150/90 (min/max/home), like the others, until calibrated.
- **[owner]** Joint 6 is **not locked** before calibration. `H` and `M 6` work with the placeholder limits. The owner accepts that the first move is within 30–150 and follows the calibration procedure (one servo at a time, hand on the PSU switch).

### Pins

- **ESP32 D1 R32: GPIO 17 (D4). Amended 2026-11-27: the owner chose D4 over GPIO33.** GPIO33 has no usable header label on the D1 R32 (its pad is unpopulated beside the power header), so it would need soldering. GPIO17 is an output-capable pin, not a strap pin, not input-only. Label source: a third-party pinout page (espboards.dev), to be checked against the owner's silkscreen. The firmware uses the GPIO number, as `docs/wiring/README.md` requires.
- **Arduino Uno: D11 [my call].** PWM pin, not used by HC-05 (D0/D1) or any other signal.
- **ESP8266: not used.** No joint 6 (see Constraints).

### Board configs

- **[my call]** `kJointCount` moves out of `roboarm/src/core/Config.h` into each board's `config_*.h`: `6` for `config_esp32.h` and `config_uno.h`, `5` for `config_esp8266.h`. Core code and tests read the count from the board config they are built against.
- `homeOrder` for six joints: **[my call]** base (4), shoulder (3), elbow (2), **joint 6**, wrist (1), claw (0). The joint 6 servo homes after the elbow and before the wrist, because it sits between them. Attaching outward from the base keeps the staggered-home behaviour consistent with the current order.
- `ArmConfig` arrays grow to `kJointCount` for the board in use. Nothing else in the struct changes.

### Protocol (version 2)

- **[my call]** `kProtocolVersion` becomes **2**.
- `M <joint> <deg>`: accepts joints `1..kJointCount` for the build. On ESP8266, `M 6` → `ERR bad joint`. Unchanged for joints 1–5.
- `A <d1> ... <dN>`: now takes **N values**, where N is the board's `kJointCount`. On a six-joint build, `A` with five values → `ERR bad command` (no motion). This is the fail-safe for an old app.
- `STATE <d1> ... <dN> <attached 0/1> <speed>`: now prints N positions. Existing field order is unchanged; joint 6 is inserted as the last position before `attached`.
- `OK A <d1> ... <dN>`: prints N values, same change as `A`.
- `I` (INFO) reply gains a count field: `INFO RoboArm fw=... proto=2 board=... wiring=... joints=N`. **[my call]** This is how the app learns the count (6 or 5) instead of guessing.
- `V`, `H`, `S`, `R`, `?` are unchanged.
- Version string: `kFirmwareVersion` bumps to `0.3.0` **[my call]** (min firmware for the new wiring).

### Wiring documents

- **`docs/wiring/esp32-d1r32.svg`**: wiring **v2**. Add a sixth servo box between the J2 and J3 blocks, with brown/red/orange wires to the rail and to GPIO17 (D4). Update the title block, the signal-pin label list and the pin table in `docs/wiring/README.md`.
- **`docs/wiring/uno-hc05.svg`**: wiring **v2**. Same for D11. HC-05 wiring unchanged.
- **`docs/wiring/esp8266-d1.svg`**: unchanged. Stays wiring **v1** with five joints; the ESP8266 section of the README states this explicitly.
- **`docs/wiring/CHANGELOG.md`**: add v2 entries for `esp32-d1r32` and `uno`, with date, reason ("sixth servo, joint 6") and min firmware 0.3.0.
- `config_esp32.h` and `config_uno.h` `wiringVersion` → `2`. `config_esp8266.h` stays `1`.
- The pre-power checklist in `docs/wiring/README.md` gains one step: check the sixth servo's brown/red/orange wires and that it sits on the same servo rail and capacitor as the others, before `H`. **[my call]** No new power budget number is set here: the existing PSU rules apply (6.0 V max, 3 A first limit, 5 A later). Whether six servos need a second capacitor or a larger PSU is an open item.

### Calibration

- `docs/calibration.md`: add joint 6 to the one-servo-at-a-time procedure, mechanically free before testing. Limits are set by the owner. The 30/150/90 placeholders are a starting point, not calibrated values (joint 6 is not locked, see Decisions).

### Tests

- `test/test_protocol.cpp`: `M 6` accepted on six-joint build; `A` with five values rejected on six-joint build; `A` with six values accepted; `STATE` and `OK A` print six positions.
- `test/test_arm.cpp`: joint 6 homes after elbow and before wrist; `M 6` moves joint 6 and stays inside its limits; release detaches joint 6; info reply contains `joints=6` on the six-joint config.
- `test/test_protocol.cpp` and `test/test_arm.cpp` keep the five-joint cases behind an ESP8266-style test config so the ESP8266 behaviour (`M 6` → bad joint) is still covered.
- Existing tests that hard-code five values are updated, not deleted.

### Hardware verification (owner)

- Connect only joint 6 first, following the calibration procedure, with the PSU current limit at 3 A.
- Confirm `I` shows `joints=6` and `wiring=2` before the PSU is turned on.
- Confirm `?` returns six positions, and `A` with five values is rejected.

## Open items

- **Companion app (`roboarm-control`).** It must parse six `STATE` and `A` values, read `joints=`, and refuse to send `A` with the wrong count. Out of this repo; it must be done before the owner uses the new firmware with the app.
- **Joint 6 function and label.** The owner needs to confirm what the servo does (lift, roll, or something else) for the label and the calibration notes.
- **Board scope.** I've assumed the ESP8266 build stays at five joints. The owner should confirm this is acceptable.
- **Header labels.** D4 (GPIO17) and D11 must be checked on the owner's silkscreen. The GPIO number is what the firmware uses.
- **Power budget for six servos.** No current figure is set. Owner to decide whether the existing rail and one capacitor are enough, or whether a second capacitor or a larger PSU is needed.

## What's left to the builder

- Exact SVG layout for the new servo box (follow the existing style of the J2 and J3 blocks).
- Exact test case names and helper structure.
- Whether `kJointCount` is a `constexpr` in each board header or passed through a template parameter, as long as the core compiles for each board.
