#include <string.h>
#include "core/LineReader.h"
#include "core/Protocol.h"
#include "check.h"

static LineReader::Result feedStr(LineReader& r, const char* s) {
  LineReader::Result last = LineReader::kNone;
  for (; *s; ++s) last = r.feed(*s);
  return last;
}

TEST(line_basic) {
  LineReader r;
  CHECK_EQ(feedStr(r, "M 3 110\n"), LineReader::kLine);
  CHECK_STR(r.line(), "M 3 110");
}
TEST(line_crlf) {
  LineReader r;
  CHECK_EQ(feedStr(r, "?\r\n"), LineReader::kLine);
  CHECK_STR(r.line(), "?");
}
TEST(line_no_result_before_newline) {
  LineReader r;
  CHECK_EQ(feedStr(r, "M 3"), LineReader::kNone);
}
TEST(line_empty) {
  LineReader r;
  CHECK_EQ(feedStr(r, "\n"), LineReader::kLine);
  CHECK_STR(r.line(), "");
}
TEST(line_exact_max_ok) {
  LineReader r;
  for (size_t i = 0; i < LineReader::kMaxLen; i++) CHECK_EQ(r.feed('x'), LineReader::kNone);
  CHECK_EQ(r.feed('\n'), LineReader::kLine);
  CHECK_EQ(strlen(r.line()), LineReader::kMaxLen);
}
TEST(line_overflow_recovers) {
  LineReader r;
  for (int i = 0; i < 60; i++) CHECK_EQ(r.feed('x'), LineReader::kNone);
  CHECK_EQ(r.feed('\n'), LineReader::kOverflow);
  CHECK_EQ(feedStr(r, "H\n"), LineReader::kLine);
  CHECK_STR(r.line(), "H");
}
TEST(line_overflow_by_one) {
  LineReader r;
  for (size_t i = 0; i < LineReader::kMaxLen + 1; i++) r.feed('x');
  CHECK_EQ(r.feed('\n'), LineReader::kOverflow);
}

static ParseError P(const char* s, Command& c) { return parseCommand(s, c); }

TEST(parse_move) {
  Command c{};
  CHECK_EQ(P("M 3 110", c), ParseError::None);
  CHECK_EQ(c.type, CmdType::Move);
  CHECK_EQ(c.joint, 3);
  CHECK_EQ(c.values[0], 110);
}
TEST(parse_all) {
  Command c{};
  CHECK_EQ(P("A 10 20 30 40 50", c), ParseError::None);
  CHECK_EQ(c.type, CmdType::All);
  for (int i = 0; i < 5; i++) CHECK_EQ(c.values[i], (i + 1) * 10);
}
TEST(parse_simple) {
  Command c{};
  CHECK_EQ(P("H", c), ParseError::None); CHECK_EQ(c.type, CmdType::Home);
  CHECK_EQ(P("S", c), ParseError::None); CHECK_EQ(c.type, CmdType::Stop);
  CHECK_EQ(P("R", c), ParseError::None); CHECK_EQ(c.type, CmdType::Release);
  CHECK_EQ(P("?", c), ParseError::None); CHECK_EQ(c.type, CmdType::Query);
  CHECK_EQ(P("V 60", c), ParseError::None);
  CHECK_EQ(c.type, CmdType::Speed);
  CHECK_EQ(c.values[0], 60);
}
TEST(parse_bad_joint) {
  Command c{};
  CHECK_EQ(P("M 0 90", c), ParseError::BadJoint);
  CHECK_EQ(P("M 6 90", c), ParseError::BadJoint);
}
TEST(parse_garbage) {
  Command c{};
  const char* bad[] = {"", "X", "M", "M 3", "M 3 abc", "A 1 2 3", "M 3 90 extra",
                       "V", "H x", "M 3 99999", "M 3 -99999", "A 1 2 3 4 5 6", "M 3x 90"};
  for (const char* s : bad) {
    if (P(s, c) != ParseError::BadCommand) check::fail(__FILE__, __LINE__, s);
  }
}
TEST(parse_spaces) {
  Command c{};
  CHECK_EQ(P("M  3   110", c), ParseError::None);
  CHECK_EQ(c.joint, 3);
  CHECK_EQ(c.values[0], 110);
  CHECK_EQ(P("  H  ", c), ParseError::None);
  CHECK_EQ(c.type, CmdType::Home);
  CHECK_EQ(P("M 2 -5", c), ParseError::None);
  CHECK_EQ(c.values[0], -5);
}
