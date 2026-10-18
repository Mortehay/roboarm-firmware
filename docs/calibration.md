# Calibration (owner procedure)

Goal: find a safe **min**, **max** and **home** for each joint and enter them in `roboarm/src/boards/config_esp32.h` (or `roboarm/src/boards/config_uno.h` if you use the Uno; use `/dev/ttyACM0` instead of `/dev/ttyUSB0` for the commands below, and unplug the HC-05 TXD wire for USB work). Initial values are min 30, max 150, home 90 for every joint, which are guesses.

**Degrees here are command values, not true horn angles.** The servo library maps 0 to 180 onto 500 to 2400 µs pulses, and real servos differ. Record the numbers you send, not what a protractor shows. Calibration measures where the arm physically stops being happy, in those command units.

## Before you start

- Follow the [pre-power checklist](wiring/README.md). Supply 5.5 V, current limit 3 A.
- Work **one servo at a time**. Wire only the servo under test (the others unplugged) at first, with its horn **mechanically free**, or the arm positioned so the joint can move without hitting anything.
- Keep a hand on the PSU output switch. Watch the PSU current display: a value that climbs or hits the limit means the joint is pushing against something. Stop.
- Hand-position the joint near the middle of its range before pressing Home. On attach the servo jumps at full speed to the home value.

## Step 1: first power-up with the claw only (J1)

1. Connect only the claw servo (J1), with the checklist complete.
2. Send `H`. The claw attaches and moves to home.
3. Send `M 1 60`, then `M 1 120`. The motion should be smooth and the PSU current should stay **below 1 A**. If not, stop and re-check wiring.

## Step 2: find the limits of each joint

For each joint (J1 to J5), one at a time:

1. Send `H`, then jog with the control app or via the monitor with `M j d`.
2. Move in **5° steps** toward the low extreme, e.g. `M 1 85`, `M 1 80`, ... Pause after each step.
3. **Stop at the first sign of strain**: servo buzzing or humming, the arm touching a stop, a cable pulling, a rising PSU current. Back off 5°. That value is the joint's **min**. Record it.
4. Return to the middle and repeat toward the high extreme to find **max**.
5. Choose **home** near the arm's natural resting pose, safely inside min and max. Keep the arm balanced so gravity does not load the weak joints.

**The firmware clamps to the current min/max (30 and 150 initially).** Jogging cannot go beyond them, and you will see `WARN j clamped to d`. Either accept 30 to 150 as the outer bound for that joint, or, if the joint can clearly travel further, temporarily widen that joint's min/max in the config (for example 10 and 170), reflash, then continue carefully in 5° steps with the PSU on the 3 A limit. Set the final values afterwards.

Notes:
- The limits are firmware clamps. Leave a little margin (a few degrees) from the true mechanical stop.
- The firmware clamps commands to min..max and replies `WARN j clamped to d`.
- Calibrate the heavy joints (J3 to J5, MG996) with the arm supported so it cannot fall when released.

## Record the results

| Joint | Name | Servo | min | max | home |
| --- | --- | --- | --- | --- | --- |
| J1 | Claw | MG90 | | | |
| J2 | Wrist | MG90 | | | |
| J3 | Elbow | MG996 | | | |
| J4 | Shoulder | MG996 | | | |
| J5 | Base | MG996 | | | |

## Step 3: enter the values

Edit `roboarm/src/boards/config_esp32.h`. Each joint line is `{pin, min, max, home}`:

```
{13, 30, 150, 90},   // J1 Claw  -> {13, <min>, <max>, <home>}
```

Change only min, max and home. Do not change the pins.

## Step 4: rebuild and verify

```
make test && make build-esp32
make flash-esp32 PORT=/dev/ttyUSB0
```

Hand-position the arm near the new home pose, PSU on, then send `H`. Confirm the arm attaches one joint at a time (base, shoulder, elbow, wrist, claw) and settles at the new home. Then check each joint moves smoothly to its new min and max with `M j d`.

## Step 5: commit and sync the app

Commit the firmware change (`chore: calibrated joint limits`). Then copy the same min, max and home values into the control app's `joints.py` in the `roboarm-control` repo and commit there too, so the sliders match the firmware.
