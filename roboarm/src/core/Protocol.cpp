#include "Protocol.h"
#include <stdlib.h>
#include <errno.h>

namespace {

bool isSpace(char c) { return c == ' ' || c == '\t'; }

const char* skipSpaces(const char* p) {
  while (isSpace(*p)) p++;
  return p;
}

// Parse a whole-token integer in int16 range. Advances p past the token.
bool parseInt16(const char*& p, int16_t& v) {
  p = skipSpaces(p);
  if (*p == '\0') return false;
  char* end = nullptr;
  errno = 0;
  long n = strtol(p, &end, 10);
  if (end == p || errno != 0) return false;
  if (*end != '\0' && !isSpace(*end)) return false;
  if (n < INT16_MIN || n > INT16_MAX) return false;
  v = static_cast<int16_t>(n);
  p = end;
  return true;
}

bool atEnd(const char* p) { return *skipSpaces(p) == '\0'; }

}  // namespace

ParseError parseCommand(const char* line, Command& out) {
  const char* p = skipSpaces(line);
  char cmd = *p;
  if (cmd == '\0') return ParseError::BadCommand;
  p++;
  if (*p != '\0' && !isSpace(*p)) return ParseError::BadCommand;

  out.joint = 0;
  for (uint8_t i = 0; i < kJointCount; i++) out.values[i] = 0;

  switch (cmd) {
    case 'H': out.type = CmdType::Home; return atEnd(p) ? ParseError::None : ParseError::BadCommand;
    case 'S': out.type = CmdType::Stop; return atEnd(p) ? ParseError::None : ParseError::BadCommand;
    case 'R': out.type = CmdType::Release; return atEnd(p) ? ParseError::None : ParseError::BadCommand;
    case '?': out.type = CmdType::Query; return atEnd(p) ? ParseError::None : ParseError::BadCommand;
    case 'V': {
      out.type = CmdType::Speed;
      if (!parseInt16(p, out.values[0]) || !atEnd(p)) return ParseError::BadCommand;
      return ParseError::None;
    }
    case 'M': {
      out.type = CmdType::Move;
      int16_t j = 0;
      if (!parseInt16(p, j) || !parseInt16(p, out.values[0]) || !atEnd(p))
        return ParseError::BadCommand;
      if (j < 1 || j > kJointCount) return ParseError::BadJoint;
      out.joint = static_cast<uint8_t>(j);
      return ParseError::None;
    }
    case 'A': {
      out.type = CmdType::All;
      for (uint8_t i = 0; i < kJointCount; i++)
        if (!parseInt16(p, out.values[i])) return ParseError::BadCommand;
      return atEnd(p) ? ParseError::None : ParseError::BadCommand;
    }
    default: return ParseError::BadCommand;
  }
}
