// Checks the real board config for the selected BOARD_* against the core, built with the
// same platform defines as the sketch. Built once per board by the `test-boards` Makefile target.
#include <string>
#include <vector>
#include "core/Arm.h"
#include "check.h"

#if defined(BOARD_ESP32)
#include "boards/config_esp32.h"
constexpr int kExpectJoints = 6;
constexpr int kExpectWiring = 2;
const uint8_t kExpectPins[6] = {13, 14, 27, 25, 26, 17};
const uint8_t kExpectHome[6] = {4, 3, 2, 5, 1, 0};  // base, shoulder, elbow, wrist1, wrist0, claw
#elif defined(BOARD_UNO)
#include "boards/config_uno.h"
constexpr int kExpectJoints = 6;
constexpr int kExpectWiring = 2;
const uint8_t kExpectPins[6] = {3, 5, 6, 9, 10, 11};
const uint8_t kExpectHome[6] = {4, 3, 2, 5, 1, 0};
#elif defined(BOARD_ESP8266)
#include "boards/config_esp8266.h"
constexpr int kExpectJoints = 5;
constexpr int kExpectWiring = 1;
const uint8_t kExpectPins[5] = {13, 12, 14, 4, 5};
const uint8_t kExpectHome[5] = {4, 3, 2, 1, 0};
#else
#error "define BOARD_ESP32, BOARD_UNO or BOARD_ESP8266"
#endif

namespace {
struct NullServo : ServoOut {
  void attach(uint8_t) override {}
  void detach(uint8_t) override {}
  void write(uint8_t, int16_t) override {}
};
struct VecSink : ReplySink {
  std::vector<std::string> lines;
  void line(const char* t) override { lines.push_back(t); }
};
std::string reply(Arm& a, const std::string& l) {
  VecSink s; a.handleLine(l.c_str(), s);
  return s.lines.empty() ? "" : s.lines.back();
}
}  // namespace

TEST(board_joint_count_and_wiring) {
  CHECK_EQ((int)kJointCount, kExpectJoints);
  CHECK_EQ((int)kConfig.wiringVersion, kExpectWiring);
}

TEST(board_pins_match_wiring_docs) {
  for (int i = 0; i < kExpectJoints; i++) CHECK_EQ((int)kConfig.joints[i].pin, (int)kExpectPins[i]);
}

TEST(board_home_order_matches_spec) {
  for (int i = 0; i < kExpectJoints; i++) CHECK_EQ((int)kConfig.homeOrder[i], (int)kExpectHome[i]);
}

TEST(board_core_agrees_with_board) {
  NullServo out;
  Arm a(kConfig, out);
  std::string info = reply(a, "I");
  CHECK(info.find("joints=" + std::to_string(kExpectJoints)) != std::string::npos);
  // one joint past the board's count is rejected
  std::string bad = "M " + std::to_string(kExpectJoints + 1) + " 90";
  CHECK_STR(reply(a, bad).c_str(), "ERR bad joint");
  // a full pose for this board is accepted by the parser (arm is detached, so the state check answers)
  std::string pose = "A";
  for (int i = 0; i < kExpectJoints; i++) pose += " 90";
  CHECK_STR(reply(a, pose).c_str(), "ERR not armed");
  // one value short is a parse error
  std::string shortPose = "A";
  for (int i = 0; i < kExpectJoints - 1; i++) shortPose += " 90";
  CHECK_STR(reply(a, shortPose).c_str(), "ERR bad command");
}
