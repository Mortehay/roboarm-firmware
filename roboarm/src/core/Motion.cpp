#include "Motion.h"

Motion::Motion() : speed_(60.0f), lastMs_(0), started_(false) {
  for (uint8_t i = 0; i < kJointCount; ++i) pos_[i] = tgt_[i] = 0.0f;
}

void Motion::reset(uint8_t j, float deg) {
  if (j >= kJointCount) return;
  pos_[j] = tgt_[j] = deg;
}

void Motion::setTarget(uint8_t j, float deg) {
  if (j >= kJointCount) return;
  tgt_[j] = deg;
}

void Motion::setSpeed(float degPerSec) { speed_ = degPerSec; }

void Motion::holdAll() {
  for (uint8_t i = 0; i < kJointCount; ++i) tgt_[i] = pos_[i];
}

void Motion::update(uint32_t nowMs) {
  if (!started_) {
    started_ = true;
    lastMs_ = nowMs;
    return;
  }
  uint32_t dt = nowMs - lastMs_;  // wraps correctly
  lastMs_ = nowMs;
  float step = speed_ * (static_cast<float>(dt) / 1000.0f);
  for (uint8_t i = 0; i < kJointCount; ++i) {
    float d = tgt_[i] - pos_[i];
    if (d > step) pos_[i] += step;
    else if (d < -step) pos_[i] -= step;
    else pos_[i] = tgt_[i];
  }
}

float Motion::position(uint8_t j) const { return j < kJointCount ? pos_[j] : 0.0f; }
float Motion::target(uint8_t j) const { return j < kJointCount ? tgt_[j] : 0.0f; }

bool Motion::atTarget(uint8_t j) const {
  if (j >= kJointCount) return true;
  float d = pos_[j] - tgt_[j];
  return (d < 0 ? -d : d) < 0.5f;
}
