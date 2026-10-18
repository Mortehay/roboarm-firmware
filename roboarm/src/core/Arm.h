#pragma once
#include <stdint.h>
#include "Config.h"
#include "Motion.h"

class ServoOut {
 public:
  virtual void attach(uint8_t j) = 0;
  virtual void detach(uint8_t j) = 0;
  virtual void write(uint8_t j, int16_t deg) = 0;
};

class ReplySink {
 public:
  virtual void line(const char* text) = 0;
};

enum class ArmState : uint8_t { Detached, Homing, Armed };

class Arm {
 public:
  Arm(const ArmConfig& cfg, ServoOut& out);
  void handleLine(const char* line, ReplySink& reply);
  void update(uint32_t nowMs);
  ArmState state() const { return state_; }

 private:
  int16_t clampJoint(uint8_t j, int16_t deg) const;
  void attachHome(uint8_t j);
  void detachAll(bool onlyAttached);
  void finishHoming();
  void startHome();

  const ArmConfig& cfg_;
  ServoOut& out_;
  Motion motion_;
  ArmState state_;
  float userSpeed_;
  bool attached_[kJointCount];
  int16_t lastWritten_[kJointCount];
  // homing bookkeeping
  bool homingFromArmed_;
  bool homeTimed_;       // t0 recorded (Detached-origin homing)
  uint32_t homeT0_;
  uint8_t homeStep_;     // joints attached (detached-origin) / current joint (armed-origin)
  bool homeStepStarted_;
};
