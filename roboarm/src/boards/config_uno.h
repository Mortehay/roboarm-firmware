#pragma once
#include "../core/Config.h"

// Uno pins. Joints: 1 Claw, 2 Wrist0, 3 Elbow, 4 Shoulder, 5 Base, 6 Wrist1.
// Limits are initial values until calibrated (Task B7).
const ArmConfig kConfig = {
    {{3, 30, 150, 90},
     {5, 30, 150, 90},
     {6, 30, 150, 90},
     {9, 30, 150, 90},
     {10, 30, 150, 90},
     {11, 30, 150, 90}},
    {4, 3, 2, 5, 1, 0},  // homeOrder: base, shoulder, elbow, wrist1, wrist0, claw
    60.0f,            // defaultSpeed deg/s
    30.0f,            // homingSpeed deg/s
    300,              // settleMs
    "uno",            // boardName
    2};               // wiringVersion
