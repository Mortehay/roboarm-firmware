#pragma once
#include <stddef.h>
#include <stdint.h>

class LineReader {
 public:
  static constexpr size_t kMaxLen = 48;
  enum Result : uint8_t { kNone, kLine, kOverflow };

  // '\r' ignored; '\n' completes a line. More than kMaxLen chars before '\n'
  // are discarded and kOverflow is returned at that '\n', then state resets.
  Result feed(char c);
  // Valid after kLine until the next feed().
  const char* line() const { return buf_; }

 private:
  char buf_[kMaxLen + 1] = {0};
  size_t len_ = 0;
  bool overflow_ = false;
};
