#include <math.h>
#include "core/Motion.h"
#include "check.h"

TEST(motion_moves_at_speed) {
  Motion m;
  m.reset(0, 90);
  m.setSpeed(60);
  m.setTarget(0, 150);
  m.update(0);
  m.update(500);
  CHECK(fabs(m.position(0) - 120.0f) < 0.01);
  CHECK(!m.atTarget(0));
  m.update(1500);
  CHECK(fabs(m.position(0) - 150.0f) < 0.01);
  CHECK(m.atTarget(0));
}
TEST(motion_no_overshoot) {
  Motion m;
  m.reset(0, 90);
  m.setSpeed(60);
  m.setTarget(0, 91);
  m.update(0);
  m.update(1000);
  CHECK_EQ(m.position(0), 91.0f);
}
TEST(motion_downward) {
  Motion m;
  m.reset(0, 90);
  m.setSpeed(60);
  m.setTarget(0, 30);
  m.update(0);
  m.update(500);
  CHECK(fabs(m.position(0) - 60.0f) < 0.01);
}
TEST(motion_hold) {
  Motion m;
  m.reset(0, 90);
  m.setSpeed(60);
  m.setTarget(0, 150);
  m.update(0);
  m.update(500);
  m.holdAll();
  CHECK(fabs(m.target(0) - m.position(0)) < 0.0001);
  float p = m.position(0);
  m.update(2000);
  CHECK_EQ(m.position(0), p);
}
TEST(motion_millis_wrap) {
  Motion m;
  m.reset(0, 90);
  m.setSpeed(60);
  m.setTarget(0, 150);
  m.update(0xFFFFFF00u);
  m.update(0x00000100u);
  CHECK(fabs(m.position(0) - 120.72f) < 0.01);
}
TEST(motion_default_speed_and_first_update) {
  Motion m;
  CHECK(fabs(m.speed() - 60.0f) < 0.0001);
  m.reset(1, 90);
  m.setTarget(1, 150);
  m.update(5000);
  CHECK_EQ(m.position(1), 90.0f);
  m.setSpeed(120);
  CHECK(fabs(m.speed() - 120.0f) < 0.0001);
}
