#include "check.h"

int main() {
  int total = 0;
  int failedTests = 0;
  for (check::Test* t = check::head(); t; t = t->next) {
    int before = check::failures();
    t->fn();
    total++;
    if (check::failures() != before) {
      failedTests++;
      printf("  in test: %s\n", t->name);
    }
  }
  printf("%d passed, %d failed\n", total - failedTests, failedTests);
  return failedTests ? 1 : 0;
}
