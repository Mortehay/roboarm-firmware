#pragma once
#include <stdint.h>
#include "Config.h"

// Speed-limited motion toward per-joint targets. No hardware access.
class Motion {
 public:
  Motion();
  void reset(uint8_t j, float deg);  // position = target = deg (no motion)
  void setTarget(uint8_t j, float deg);
  void setSpeed(float degPerSec);
  float speed() const { return speed_; }
  void holdAll();                    // target = current position
  void update(uint32_t nowMs);       // first call only records time
  float position(uint8_t j) const;
  float target(uint8_t j) const;
  bool atTarget(uint8_t j) const;    // |pos-target| < 0.5

 private:
  float pos_[kJointCount];
  float tgt_[kJointCount];
  float speed_;
  uint32_t lastMs_;
  bool started_;
};
