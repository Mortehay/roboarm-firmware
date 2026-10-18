#include <string>
#include <vector>
#include "core/Arm.h"
#include "check.h"

#define CHECK_CMD(arm, line, expected) \
  do { std::string rep_ = cmd(arm, line); CHECK_STR(rep_.c_str(), expected); } while (0)

namespace {
struct Call { char kind; int j; int deg; };  // 'a','d','w'
struct FakeServoOut : ServoOut {
  std::vector<Call> calls;
  void attach(uint8_t j) override { calls.push_back({'a', j, 0}); }
  void detach(uint8_t j) override { calls.push_back({'d', j, 0}); }
  void write(uint8_t j, int16_t deg) override { calls.push_back({'w', j, deg}); }
  int count(char k) const { int n = 0; for (auto& c : calls) if (c.kind == k) ++n; return n; }
  int lastWrite(int j) const {
    for (size_t i = calls.size(); i-- > 0;) if (calls[i].kind == 'w' && calls[i].j == j) return calls[i].deg;
    return -1;
  }
};
struct VecSink : ReplySink {
  std::vector<std::string> lines;
  void line(const char* t) override { lines.push_back(t); }
};

ArmConfig makeCfg() {
  ArmConfig c{};
  for (auto& j : c.joints) { j.pin = 1; j.minDeg = 30; j.maxDeg = 150; j.homeDeg = 90; }
  const uint8_t order[kJointCount] = {4, 3, 2, 1, 0};
  for (int i = 0; i < kJointCount; ++i) c.homeOrder[i] = order[i];
  c.defaultSpeed = 60; c.homingSpeed = 30; c.settleMs = 300;
  return c;
}
std::string cmd(Arm& a, const char* l) {
  VecSink s; a.handleLine(l, s);
  return s.lines.empty() ? "" : s.lines.back();
}
uint32_t gNow;
void run(Arm& a, uint32_t ms) {  // step 10 ms
  for (uint32_t t = 0; t < ms; t += 10) { gNow += 10; a.update(gNow); }
}
void arm(Arm& a) {  // detached -> armed
  gNow = 0;
  cmd(a, "H");
  a.update(0);
  run(a, 1600);
}
}  // namespace

TEST(boot_detached) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  CHECK(a.state() == ArmState::Detached);
  CHECK_EQ(o.count('a'), 0);
  CHECK_CMD(a, "?", "STATE 90 90 90 90 90 0 60");
}
TEST(move_rejected_when_detached) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  CHECK_CMD(a, "M 3 100", "ERR not armed");
  CHECK_CMD(a, "A 90 90 90 90 90", "ERR not armed");
  CHECK_EQ(o.count('w'), 0);
}
TEST(home_staggered) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  CHECK_CMD(a, "H", "OK H");
  CHECK(a.state() == ArmState::Homing);
  CHECK_EQ(o.count('a'), 0);
  a.update(0);
  CHECK_EQ(o.count('a'), 1);
  CHECK_EQ(o.calls[0].j, 4);
  CHECK_EQ(o.lastWrite(4), 90);
  a.update(299);
  CHECK_EQ(o.count('a'), 1);
  a.update(300);
  CHECK_EQ(o.count('a'), 2);
  CHECK_EQ(o.calls.back().j, 3);
  a.update(600); a.update(900);
  CHECK_EQ(o.count('a'), 4);
  a.update(1200);
  CHECK_EQ(o.count('a'), 5);
  CHECK(a.state() == ArmState::Homing);
  a.update(1499);
  CHECK(a.state() == ArmState::Homing);
  a.update(1500);
  CHECK(a.state() == ArmState::Armed);
  CHECK_EQ(o.count('w'), 5);
}
TEST(home_t0_is_first_update_not_H) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  cmd(a, "H");
  a.update(5000);
  a.update(5299);
  CHECK_EQ(o.count('a'), 1);
  a.update(5300);
  CHECK_EQ(o.count('a'), 2);
}
TEST(move_rejected_while_homing) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  cmd(a, "H"); a.update(0);
  CHECK_CMD(a, "M 1 90", "ERR homing");
  CHECK_CMD(a, "A 90 90 90 90 90", "ERR homing");
  CHECK_CMD(a, "H", "OK H");
  CHECK_EQ(o.count('a'), 1);  // no restart
}
TEST(move_and_clamp) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  CHECK(a.state() == ArmState::Armed);
  VecSink s; a.handleLine("M 3 170", s);
  CHECK_EQ(s.lines.size(), (size_t)2);
  CHECK_STR(s.lines[0].c_str(), "WARN 3 clamped to 150");
  CHECK_STR(s.lines[1].c_str(), "OK M 3 150");
  run(a, 2000);
  CHECK_EQ(o.lastWrite(2), 150);
  VecSink s2; a.handleLine("M 3 100", s2);
  CHECK_EQ(s2.lines.size(), (size_t)1);
  CHECK_STR(s2.lines[0].c_str(), "OK M 3 100");
}
TEST(move_all) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  VecSink s; a.handleLine("A 10 90 90 90 90", s);
  CHECK_EQ(s.lines.size(), (size_t)2);
  CHECK_STR(s.lines[0].c_str(), "WARN 1 clamped to 30");
  CHECK_STR(s.lines[1].c_str(), "OK A 30 90 90 90 90");
}
TEST(stop_holds) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  cmd(a, "M 1 150");
  run(a, 300);
  CHECK_CMD(a, "S", "OK S");
  std::string p1 = cmd(a, "?");
  run(a, 1000);
  std::string p2 = cmd(a, "?");
  CHECK_STR(p2.c_str(), p1.c_str());
  CHECK_EQ(o.count('d'), 0);
  CHECK(a.state() == ArmState::Armed);
}
TEST(release_detaches) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  CHECK_CMD(a, "R", "OK R");
  CHECK_EQ(o.count('d'), 5);
  CHECK(a.state() == ArmState::Detached);
  CHECK_CMD(a, "?", "STATE 90 90 90 90 90 0 60");
  int w = o.count('w');
  run(a, 500);
  CHECK_EQ(o.count('w'), w);
}
TEST(no_detach_without_R) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  cmd(a, "M 2 120");
  run(a, 10000);
  CHECK_EQ(o.count('d'), 0);
}
TEST(speed) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  CHECK_CMD(a, "V 4", "ERR bad speed");
  CHECK_CMD(a, "V 181", "ERR bad speed");
  CHECK_CMD(a, "V 120", "OK V 120");
  CHECK_CMD(a, "?", "STATE 90 90 90 90 90 0 120");
}
TEST(bad_lines) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  CHECK_CMD(a, "zzz", "ERR bad command");
  CHECK_CMD(a, "M 9 90", "ERR bad joint");
}
TEST(stop_while_homing_detaches) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  cmd(a, "H"); a.update(0); a.update(300);
  CHECK_CMD(a, "S", "OK S");
  CHECK_EQ(o.count('d'), 2);
  CHECK(a.state() == ArmState::Detached);
  a.update(2000);
  CHECK_EQ(o.count('a'), 2);
}
TEST(rehome_from_armed_sequential) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  cmd(a, "V 100");
  cmd(a, "A 60 60 60 60 60");
  run(a, 1000);
  CHECK_CMD(a, "H", "OK H");
  CHECK(a.state() == ArmState::Homing);
  CHECK_CMD(a, "V 150", "OK V 150");
  gNow += 10; a.update(gNow);
  // 30 deg/s: after 0.5 s joint index 4 has moved ~15, others untouched
  run(a, 500);
  CHECK_EQ(o.lastWrite(4) > 60, true);
  CHECK_EQ(o.lastWrite(3), 60);
  run(a, 6000);
  CHECK(a.state() == ArmState::Armed);
  CHECK_CMD(a, "?", "STATE 90 90 90 90 90 1 150");
  CHECK_EQ(o.count('d'), 0);
}

TEST(stop_during_rehome_freezes_armed) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  cmd(a, "V 100");
  cmd(a, "A 60 60 60 60 60");
  run(a, 1000);
  cmd(a, "H");
  run(a, 500);
  CHECK(a.state() == ArmState::Homing);
  CHECK_CMD(a, "S", "OK S");
  CHECK(a.state() == ArmState::Armed);
  CHECK_EQ(o.count('d'), 0);
  std::string p1 = cmd(a, "?");
  run(a, 3000);
  std::string p2 = cmd(a, "?");
  CHECK_STR(p2.c_str(), p1.c_str());
  CHECK_EQ(p2.substr(p2.size() - 5), std::string("1 100"));
  CHECK_EQ(o.count('d'), 0);
}
TEST(release_during_homing_stable) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  cmd(a, "H"); a.update(0); a.update(300);
  CHECK_CMD(a, "R", "OK R");
  CHECK_EQ(o.count('d'), 5);
  CHECK(a.state() == ArmState::Detached);
  std::string p1 = cmd(a, "?");
  int w = o.count('w');
  run(a, 3000);
  CHECK_EQ(o.count('w'), w);
  CHECK_EQ(o.count('a'), 2);
  std::string p2 = cmd(a, "?");
  CHECK_STR(p2.c_str(), p1.c_str());
  CHECK_STR(p2.c_str(), "STATE 90 90 90 90 90 0 60");
}
TEST(round_negative_half_away) {
  ArmConfig c = makeCfg();
  c.joints[0].minDeg = -50; c.joints[0].maxDeg = 150; c.joints[0].homeDeg = -10;
  FakeServoOut o; Arm a(c, o);
  CHECK_CMD(a, "?", "STATE -10 90 90 90 90 0 60");
}
