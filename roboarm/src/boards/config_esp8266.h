#pragma once
#include "../core/Config.h"

// ESP8266 (Wemos D1) pins. Joints: 1 Claw, 2 Wrist, 3 Elbow, 4 Shoulder, 5 Base.
// Limits are initial values until calibrated (Task B7).
const ArmConfig kConfig = {
    {{13, 30, 150, 90},
     {12, 30, 150, 90},
     {14, 30, 150, 90},
     {4, 30, 150, 90},
     {5, 30, 150, 90}},
    {4, 3, 2, 1, 0},  // homeOrder: base, shoulder, elbow, wrist, claw
    60.0f,            // defaultSpeed deg/s
    30.0f,            // homingSpeed deg/s
    300,              // settleMs
    "esp8266-d1",    // boardName
    1};               // wiringVersion
