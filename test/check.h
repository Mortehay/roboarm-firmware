// Minimal self-made test framework: TEST, CHECK, CHECK_EQ, CHECK_STR.
#pragma once
#include <stdio.h>
#include <string.h>

namespace check {
struct Test {
  const char* name;
  void (*fn)();
  Test* next;
};
inline Test*& head() { static Test* h = nullptr; return h; }
inline int& failures() { static int f = 0; return f; }
struct Registrar {
  Registrar(Test* t) {
    // append to preserve declaration order within a translation unit
    Test** p = &head();
    while (*p) p = &(*p)->next;
    t->next = nullptr;
    *p = t;
  }
};
inline void fail(const char* file, int line, const char* msg) {
  printf("FAIL %s:%d: %s\n", file, line, msg);
  failures()++;
}
}  // namespace check

#define TEST(name)                                                     \
  static void test_##name();                                           \
  static check::Test test_obj_##name = {#name, test_##name, nullptr};  \
  static check::Registrar test_reg_##name(&test_obj_##name);           \
  static void test_##name()

#define CHECK(expr)                                                    \
  do {                                                                 \
    if (!(expr)) check::fail(__FILE__, __LINE__, "CHECK(" #expr ")");  \
  } while (0)

#define CHECK_EQ(a, b)                                                 \
  do {                                                                 \
    if (!((a) == (b)))                                                 \
      check::fail(__FILE__, __LINE__, "CHECK_EQ(" #a ", " #b ")");     \
  } while (0)

#define CHECK_STR(a, b)                                                \
  do {                                                                 \
    const char* _a = (a);                                              \
    const char* _b = (b);                                              \
    if (!_a || !_b || strcmp(_a, _b) != 0) {                           \
      char _m[256];                                                    \
      snprintf(_m, sizeof _m, "CHECK_STR(" #a ", " #b ") got \"%s\" expected \"%s\"", \
               _a ? _a : "(null)", _b ? _b : "(null)");                \
      check::fail(__FILE__, __LINE__, _m);                             \
    }                                                                  \
  } while (0)
