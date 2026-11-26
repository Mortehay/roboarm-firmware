#include <string>
#include <vector>
#include "core/Arm.h"
#include "check.h"

// `expected` may be a literal or a temporary std::string; copy it before use.
#define CHECK_CMD(arm, line, expected) \
  do { std::string want_ = (expected); std::string rep_ = cmd(arm, line); CHECK_STR(rep_.c_str(), want_.c_str()); } while (0)

namespace {
constexpr int N = kJointCount;  // joints in this build (6, or 5 with ROBOARM_JOINT_COUNT=5)

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
  std::vector<int> attachOrder() const {
    std::vector<int> v;
    for (auto& c : calls) if (c.kind == 'a') v.push_back(c.j);
    return v;
  }
};
struct VecSink : ReplySink {
  std::vector<std::string> lines;
  void line(const char* t) override { lines.push_back(t); }
};

// Count-independent line builders: N values, first value `first`, the rest `rest`.
std::string joints(int first, int rest) {
  std::string s;
  for (int i = 0; i < N; ++i) {
    if (i) s += ' ';
    s += std::to_string(i == 0 ? first : rest);
  }
  return s;
}
// "A" followed by n values of v (n may be wrong on purpose).
std::string allCmd(int n, int v) {
  std::string s = "A";
  for (int i = 0; i < n; ++i) s += " " + std::to_string(v);
  return s;
}
std::string stateLine(int first, int rest, int attached, int speed) {
  return "STATE " + joints(first, rest) + " " + std::to_string(attached) + " " + std::to_string(speed);
}
std::string infoLine() {
  return "INFO RoboArm fw=0.3.0 proto=2 board=testboard wiring=7 joints=" + std::to_string(N);
}

ArmConfig makeCfg() {
  ArmConfig c{};
  for (auto& j : c.joints) { j.pin = 1; j.minDeg = 30; j.maxDeg = 150; j.homeDeg = 90; }
#if ROBOARM_JOINT_COUNT == 6  // preprocessor: kJointCount (constexpr) is not visible here
  const uint8_t order[kJointCount] = {4, 3, 2, 5, 1, 0};  // base, shoulder, elbow, wrist1, wrist0, claw
#else
  const uint8_t order[kJointCount] = {4, 3, 2, 1, 0};
#endif
  for (int i = 0; i < kJointCount; ++i) c.homeOrder[i] = order[i];
  c.defaultSpeed = 60; c.homingSpeed = 30; c.settleMs = 300;
  c.boardName = "testboard"; c.wiringVersion = 7;
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
  run(a, 300 * N + 100);  // settleMs (300) per joint, plus margin
}
}  // namespace

TEST(boot_detached) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  CHECK(a.state() == ArmState::Detached);
  CHECK_EQ(o.count('a'), 0);
  CHECK_CMD(a, "?", stateLine(90, 90, 0, 60).c_str());
}
TEST(move_rejected_when_detached) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  CHECK_CMD(a, "M 3 100", "ERR not armed");
  CHECK_CMD(a, allCmd(N, 90).c_str(), "ERR not armed");
  CHECK_EQ(o.count('w'), 0);
}
TEST(home_staggered) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  CHECK_CMD(a, "H", "OK H");
  CHECK(a.state() == ArmState::Homing);
  CHECK_EQ(o.count('a'), 0);
  a.update(0);
  CHECK_EQ(o.count('a'), 1);
  CHECK_EQ(o.calls[0].j, c.homeOrder[0]);
  CHECK_EQ(o.lastWrite(c.homeOrder[0]), 90);
  a.update(299);
  CHECK_EQ(o.count('a'), 1);
  a.update(300);
  CHECK_EQ(o.count('a'), 2);
  CHECK_EQ(o.calls.back().j, c.homeOrder[1]);
  for (int k = 2; k < N; ++k) {
    a.update(300 * k);
    CHECK_EQ(o.count('a'), k + 1);
  }
  a.update(300 * N - 1);
  CHECK(a.state() == ArmState::Homing);
  a.update(300 * N);
  CHECK(a.state() == ArmState::Armed);
  CHECK_EQ(o.count('w'), N);
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
TEST(homing_attaches_in_home_order) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  gNow = 0;
  cmd(a, "H");
  run(a, 2000);
  std::vector<int> seq = o.attachOrder();
  CHECK_EQ(seq.size(), (size_t)N);
  for (int i = 0; i < N && i < (int)seq.size(); ++i) CHECK_EQ(seq[i], c.homeOrder[i]);
}
TEST(move_rejected_while_homing) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  cmd(a, "H"); a.update(0);
  CHECK_CMD(a, "M 1 90", "ERR homing");
  CHECK_CMD(a, allCmd(N, 90).c_str(), "ERR homing");
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
TEST(move_clamps_last_joint) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  std::string line = "M " + std::to_string(N) + " 200";
  VecSink s; a.handleLine(line.c_str(), s);
  CHECK_EQ(s.lines.size(), (size_t)2);
  std::string warn = "WARN " + std::to_string(N) + " clamped to 150";
  CHECK_STR(s.lines[0].c_str(), warn.c_str());
  run(a, 2000);
  CHECK_EQ(o.lastWrite(N - 1), 150);
}
TEST(move_all) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  VecSink s; a.handleLine(("A " + joints(10, 90)).c_str(), s);
  CHECK_EQ(s.lines.size(), (size_t)2);
  CHECK_STR(s.lines[0].c_str(), "WARN 1 clamped to 30");
  std::string okA = "OK A " + joints(30, 90);
  CHECK_STR(s.lines[1].c_str(), okA.c_str());
}
TEST(all_with_one_value_too_few_is_rejected_without_motion) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  size_t before = o.calls.size();
  CHECK_CMD(a, allCmd(N - 1, 90).c_str(), "ERR bad command");
  CHECK_EQ(o.calls.size(), before);
}
TEST(all_with_one_value_too_many_is_rejected_without_motion) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  size_t before = o.calls.size();
  CHECK_CMD(a, allCmd(N + 1, 90).c_str(), "ERR bad command");
  CHECK_EQ(o.calls.size(), before);
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
  CHECK_EQ(o.count('d'), N);
  CHECK(a.state() == ArmState::Detached);
  CHECK_CMD(a, "?", stateLine(90, 90, 0, 60).c_str());
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
  CHECK_CMD(a, "?", stateLine(90, 90, 0, 120).c_str());
}
TEST(bad_lines) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  CHECK_CMD(a, "zzz", "ERR bad command");
  CHECK_CMD(a, "M 9 90", "ERR bad joint");
  CHECK_CMD(a, ("M " + std::to_string(N + 1) + " 90").c_str(), "ERR bad joint");
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
  cmd(a, allCmd(N, 60).c_str());
  run(a, 1000);
  CHECK_CMD(a, "H", "OK H");
  CHECK(a.state() == ArmState::Homing);
  CHECK_CMD(a, "V 150", "OK V 150");
  gNow += 10; a.update(gNow);
  // 30 deg/s: after 0.5 s the first joint in home order has moved ~15, joint 3 untouched
  run(a, 500);
  CHECK_EQ(o.lastWrite(c.homeOrder[0]) > 60, true);
  CHECK_EQ(o.lastWrite(3), 60);
  run(a, 6000);
  CHECK(a.state() == ArmState::Armed);
  CHECK_CMD(a, "?", stateLine(90, 90, 1, 150).c_str());
  CHECK_EQ(o.count('d'), 0);
}

TEST(stop_during_rehome_freezes_armed) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  cmd(a, "V 100");
  cmd(a, allCmd(N, 60).c_str());
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
  CHECK_EQ(o.count('d'), N);  // R detaches every joint, attached or not
  CHECK(a.state() == ArmState::Detached);
  std::string p1 = cmd(a, "?");
  int w = o.count('w');
  run(a, 3000);
  CHECK_EQ(o.count('w'), w);
  CHECK_EQ(o.count('a'), 2);
  std::string p2 = cmd(a, "?");
  CHECK_STR(p2.c_str(), p1.c_str());
  std::string st = stateLine(90, 90, 0, 60);
  CHECK_STR(p2.c_str(), st.c_str());
}
TEST(round_negative_half_away) {
  ArmConfig c = makeCfg();
  c.joints[0].minDeg = -50; c.joints[0].maxDeg = 150; c.joints[0].homeDeg = -10;
  FakeServoOut o; Arm a(c, o);
  CHECK_CMD(a, "?", stateLine(-10, 90, 0, 60).c_str());
}
TEST(query_reports_every_joint) {
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  arm(a);
  CHECK_CMD(a, "?", stateLine(90, 90, 1, 60).c_str());
}

TEST(info_in_every_state) {
  std::string want = infoLine();
  ArmConfig c = makeCfg(); FakeServoOut o; Arm a(c, o);
  CHECK_CMD(a, "I", want.c_str());
  CHECK_EQ(o.calls.size(), (size_t)0);
  CHECK_CMD(a, "H", "OK H");  // Homing
  CHECK(a.state() == ArmState::Homing);
  size_t before = o.calls.size();
  CHECK_CMD(a, "I", want.c_str());
  CHECK_EQ(o.calls.size(), before);
  arm(a);  // Armed
  CHECK(a.state() == ArmState::Armed);
  before = o.calls.size();
  CHECK_CMD(a, "I", want.c_str());
  CHECK_EQ(o.calls.size(), before);
  CHECK(a.state() == ArmState::Armed);
}
