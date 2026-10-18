#include "check.h"

TEST(smoke) { CHECK_EQ(1 + 1, 2); }

#include <stdint.h>
enum class Mode : uint8_t { A = 1, B = 2 };

TEST(check_str_percent) {
  CHECK_STR("100%", "100%");
  CHECK_STR("%s%d", "%s%d");
}

TEST(check_eq_types) {
  uint8_t u = 3;
  int16_t i = -4;
  CHECK_EQ(u, 3);
  CHECK_EQ(i, -4);
  CHECK_EQ(1.5f, 1.5f);
  CHECK_EQ(Mode::B, Mode::B);
}
