#pragma once
#include <stdint.h>

// Every translation unit of a build must see the same count, so the default is chosen here
// from the platform macro (set for all sources), never in the sketch alone.
#ifndef ROBOARM_JOINT_COUNT
#if defined(ESP8266)
#define ROBOARM_JOINT_COUNT 5  // no safe servo GPIO left for joint 6
#else
#define ROBOARM_JOINT_COUNT 6
#endif
#endif
constexpr uint8_t kJointCount = ROBOARM_JOINT_COUNT;  // joint number N <-> index N-1

struct JointConfig {
  uint8_t pin;
  int16_t minDeg, maxDeg, homeDeg;
};

struct ArmConfig {
  JointConfig joints[kJointCount];
  uint8_t homeOrder[kJointCount];  // joint indices
  float defaultSpeed, homingSpeed;
  uint16_t settleMs;
  const char* boardName;   // reported by the I command
  uint8_t wiringVersion;   // matches the docs/wiring title block
};
