#include "LineReader.h"

LineReader::Result LineReader::feed(char c) {
  if (c == '\r') return kNone;
  if (c == '\n') {
    if (overflow_) {
      overflow_ = false;
      len_ = 0;
      buf_[0] = '\0';
      return kOverflow;
    }
    buf_[len_] = '\0';
    len_ = 0;
    return kLine;
  }
  if (overflow_) return kNone;
  if (len_ >= kMaxLen) {
    overflow_ = true;
    return kNone;
  }
  buf_[len_++] = c;
  return kNone;
}
