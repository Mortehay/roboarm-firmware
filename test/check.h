// Minimal self-made test framework: TEST, CHECK, CHECK_EQ, CHECK_STR.
#pragma once
#include <stdio.h>
#include <string.h>
#include <type_traits>

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

// Render a value for failure messages (arithmetic and enums; else "?").
template <typename T>
inline void show(char* buf, size_t n, const T& v) {
  if constexpr (std::is_enum<T>::value) {
    show(buf, n, static_cast<typename std::underlying_type<T>::type>(v));
  } else if constexpr (std::is_floating_point<T>::value) {
    snprintf(buf, n, "%Lg", static_cast<long double>(v));
  } else if constexpr (std::is_arithmetic<T>::value) {
    if constexpr (std::is_signed<T>::value)
      snprintf(buf, n, "%lld", static_cast<long long>(v));
    else
      snprintf(buf, n, "%llu", static_cast<unsigned long long>(v));
  } else {
    snprintf(buf, n, "?");
  }
}
template <typename A, typename B>
inline void failEq(const char* file, int line, const char* sa, const char* sb,
                   const A& a, const B& b) {
  char va[64], vb[64], m[256];
  show(va, sizeof va, a);
  show(vb, sizeof vb, b);
  snprintf(m, sizeof m, "CHECK_EQ(%s, %s) got %s expected %s", sa, sb, va, vb);
  fail(file, line, m);
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
    const auto& _a = (a);                                              \
    const auto& _b = (b);                                              \
    if (!(_a == _b)) check::failEq(__FILE__, __LINE__, #a, #b, _a, _b); \
  } while (0)

#define CHECK_STR(a, b)                                                \
  do {                                                                 \
    const char* _a = (a);                                              \
    const char* _b = (b);                                              \
    if (!_a || !_b || strcmp(_a, _b) != 0) {                           \
      char _m[256];                                                    \
      snprintf(_m, sizeof _m, "CHECK_STR(%s, %s) got \"%s\" expected \"%s\"", \
               #a, #b, _a ? _a : "(null)", _b ? _b : "(null)");       \
      check::fail(__FILE__, __LINE__, _m);                             \
    }                                                                  \
  } while (0)
