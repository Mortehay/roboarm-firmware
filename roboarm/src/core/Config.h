#pragma once
#include <stdint.h>

constexpr uint8_t kJointCount = 5;  // joint number N <-> index N-1

struct JointConfig {
  uint8_t pin;
  int16_t minDeg, maxDeg, homeDeg;
};

struct ArmConfig {
  JointConfig joints[kJointCount];
  uint8_t homeOrder[kJointCount];  // joint indices
  float defaultSpeed, homingSpeed;
  uint16_t settleMs;
};
