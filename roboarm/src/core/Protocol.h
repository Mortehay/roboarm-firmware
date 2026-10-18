#pragma once
#include <stdint.h>
#include "Config.h"

enum class CmdType : uint8_t { Move, All, Speed, Home, Stop, Release, Query, Info };

struct Command {
  CmdType type;
  uint8_t joint;  // 1..kJointCount for Move
  int16_t values[kJointCount];
};

enum class ParseError : uint8_t { None, BadCommand, BadJoint };

// Parses one line (no newline). `out` is only meaningful when None is returned.
ParseError parseCommand(const char* line, Command& out);
