#pragma once
#include "../core/Config.h"

// ESP32 pins. Joints: 1 Claw, 2 Wrist0, 3 Elbow, 4 Shoulder, 5 Base, 6 Wrist1.
// Limits are initial values until calibrated (Task B7).
const ArmConfig kConfig = {
    {{13, 30, 150, 90},
     {14, 30, 150, 90},
     {27, 30, 150, 90},
     {25, 30, 150, 90},
     {26, 30, 150, 90},
     {17, 30, 150, 90}},
    {4, 3, 2, 5, 1, 0},  // homeOrder: base, shoulder, elbow, wrist1, wrist0, claw
    60.0f,            // defaultSpeed deg/s
    30.0f,            // homingSpeed deg/s
    300,              // settleMs
    "esp32-d1r32",    // boardName
    2};               // wiringVersion
