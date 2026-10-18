#include "Arm.h"
#include <stdio.h>
#include "Protocol.h"

namespace {
int16_t roundDeg(float x) {
  return static_cast<int16_t>(x >= 0.0f ? x + 0.5f : x - 0.5f);  // half away from zero
}
constexpr float kMinSpeed = 5.0f;
constexpr float kMaxSpeed = 180.0f;
}  // namespace

Arm::Arm(const ArmConfig& cfg, ServoOut& out)
    : cfg_(cfg), out_(out), state_(ArmState::Detached),
      userSpeed_(cfg.defaultSpeed), homingFromArmed_(false), homeTimed_(false),
      homeT0_(0), homeStep_(0), homeStepStarted_(false) {
  for (uint8_t i = 0; i < kJointCount; ++i) {
    motion_.reset(i, static_cast<float>(cfg.joints[i].homeDeg));
    attached_[i] = false;
    lastWritten_[i] = cfg.joints[i].homeDeg;
  }
  motion_.setSpeed(userSpeed_);
}

int16_t Arm::clampJoint(uint8_t j, int16_t deg) const {
  if (deg < cfg_.joints[j].minDeg) return cfg_.joints[j].minDeg;
  if (deg > cfg_.joints[j].maxDeg) return cfg_.joints[j].maxDeg;
  return deg;
}

void Arm::attachHome(uint8_t j) {
  float home = static_cast<float>(cfg_.joints[j].homeDeg);
  out_.attach(j);
  attached_[j] = true;
  out_.write(j, cfg_.joints[j].homeDeg);
  lastWritten_[j] = cfg_.joints[j].homeDeg;
  motion_.reset(j, home);
}

void Arm::detachAll(bool onlyAttached) {
  for (uint8_t i = 0; i < kJointCount; ++i) {
    if (onlyAttached && !attached_[i]) continue;
    out_.detach(i);
    attached_[i] = false;
  }
}

void Arm::finishHoming() {
  state_ = ArmState::Armed;
  homingFromArmed_ = false;
  motion_.setSpeed(userSpeed_);
}

void Arm::startHome() {
  homeTimed_ = false;
  homeStep_ = 0;
  homeStepStarted_ = false;
  state_ = ArmState::Homing;
}

void Arm::handleLine(const char* line, ReplySink& reply) {
  char buf[48];
  Command cmd;
  ParseError err = parseCommand(line, cmd);
  if (err == ParseError::BadJoint) { reply.line("ERR bad joint"); return; }
  if (err != ParseError::None) { reply.line("ERR bad command"); return; }

  switch (cmd.type) {
    case CmdType::Move: {
      if (state_ == ArmState::Detached) { reply.line("ERR not armed"); return; }
      if (state_ == ArmState::Homing) { reply.line("ERR homing"); return; }
      uint8_t j = cmd.joint - 1;
      int16_t d = clampJoint(j, cmd.values[0]);
      if (d != cmd.values[0]) {
        snprintf(buf, sizeof buf, "WARN %u clamped to %d", cmd.joint, d);
        reply.line(buf);
      }
      motion_.setTarget(j, d);
      snprintf(buf, sizeof buf, "OK M %u %d", cmd.joint, d);
      reply.line(buf);
      break;
    }
    case CmdType::All: {
      if (state_ == ArmState::Detached) { reply.line("ERR not armed"); return; }
      if (state_ == ArmState::Homing) { reply.line("ERR homing"); return; }
      int16_t d[kJointCount];
      for (uint8_t i = 0; i < kJointCount; ++i) {
        d[i] = clampJoint(i, cmd.values[i]);
        if (d[i] != cmd.values[i]) {
          snprintf(buf, sizeof buf, "WARN %u clamped to %d", i + 1, d[i]);
          reply.line(buf);
        }
        motion_.setTarget(i, d[i]);
      }
      snprintf(buf, sizeof buf, "OK A %d %d %d %d %d", d[0], d[1], d[2], d[3], d[4]);
      reply.line(buf);
      break;
    }
    case CmdType::Speed: {
      float s = static_cast<float>(cmd.values[0]);
      if (s < kMinSpeed || s > kMaxSpeed) { reply.line("ERR bad speed"); return; }
      userSpeed_ = s;
      if (!(state_ == ArmState::Homing && homingFromArmed_)) motion_.setSpeed(s);
      snprintf(buf, sizeof buf, "OK V %d", cmd.values[0]);
      reply.line(buf);
      break;
    }
    case CmdType::Home: {
      if (state_ == ArmState::Detached) {
        startHome();
        homingFromArmed_ = false;
      } else if (state_ == ArmState::Armed) {
        startHome();
        homingFromArmed_ = true;
        motion_.holdAll();
        motion_.setSpeed(cfg_.homingSpeed);
      }
      reply.line("OK H");
      break;
    }
    case CmdType::Stop: {
      if (state_ == ArmState::Armed) {
        motion_.holdAll();
      } else if (state_ == ArmState::Homing) {
        motion_.holdAll();
        motion_.setSpeed(userSpeed_);
        if (homingFromArmed_) {
          // all joints attached: freeze in place, never go limp
          finishHoming();
        } else {
          detachAll(true);
          state_ = ArmState::Detached;
        }
      }
      reply.line("OK S");
      break;
    }
    case CmdType::Release: {
      detachAll(false);
      homingFromArmed_ = false;
      motion_.holdAll();
      motion_.setSpeed(userSpeed_);
      state_ = ArmState::Detached;
      reply.line("OK R");
      break;
    }
    case CmdType::Query: {
      snprintf(buf, sizeof buf, "STATE %d %d %d %d %d %d %d",
               roundDeg(motion_.position(0)), roundDeg(motion_.position(1)),
               roundDeg(motion_.position(2)), roundDeg(motion_.position(3)),
               roundDeg(motion_.position(4)),
               state_ == ArmState::Armed ? 1 : 0, static_cast<int>(userSpeed_ + 0.5f));
      reply.line(buf);
      break;
    }
  }
}

void Arm::update(uint32_t nowMs) {
  motion_.update(nowMs);

  if (state_ == ArmState::Homing) {
    if (!homingFromArmed_) {
      if (!homeTimed_) { homeTimed_ = true; homeT0_ = nowMs; }
      uint32_t el = nowMs - homeT0_;
      while (homeStep_ < kJointCount &&
             el >= static_cast<uint32_t>(homeStep_) * cfg_.settleMs) {
        attachHome(cfg_.homeOrder[homeStep_]);
        ++homeStep_;
      }
      if (el >= static_cast<uint32_t>(kJointCount) * cfg_.settleMs) finishHoming();
    } else {
      // Armed-origin: one joint at a time, in homeOrder.
      while (homeStep_ < kJointCount) {
        uint8_t j = cfg_.homeOrder[homeStep_];
        if (!homeStepStarted_) {
          motion_.setTarget(j, static_cast<float>(cfg_.joints[j].homeDeg));
          homeStepStarted_ = true;
        }
        if (!motion_.atTarget(j)) break;
        ++homeStep_;
        homeStepStarted_ = false;
      }
      if (homeStep_ >= kJointCount) finishHoming();
    }
  }

  for (uint8_t i = 0; i < kJointCount; ++i) {
    if (!attached_[i]) continue;
    int16_t v = roundDeg(motion_.position(i));
    if (v != lastWritten_[i]) {
      out_.write(i, v);
      lastWritten_[i] = v;
    }
  }
}
