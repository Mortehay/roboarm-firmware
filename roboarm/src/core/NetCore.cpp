#include "NetCore.h"
#include <stdio.h>
#include <string.h>

bool keyLineMatches(const char* line, const char* key) {
  size_t klen = strlen(key);
  if (klen == 0) return false;  // an empty key never matches
  if (strlen(line) != klen + 2) return false;
  if (line[0] != 'K' || line[1] != ' ') return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < klen; ++i)
    diff |= static_cast<uint8_t>(line[2 + i] ^ key[i]);
  return diff == 0;
}

bool isDiscoveryRequest(const char* data, size_t len) {
  static const char kReq[] = "ROBOARM?";
  const size_t base = sizeof(kReq) - 1;
  if (len < base || memcmp(data, kReq, base) != 0) return false;
  size_t rest = len - base;
  if (rest == 0) return true;
  if (rest == 1) return data[base] == '\n';
  return rest == 2 && data[base] == '\r' && data[base + 1] == '\n';
}

size_t formatDiscoveryReply(char* buf, size_t n, const char* ip) {
  if (n == 0) return 0;
  int w = snprintf(buf, n, "ROBOARM RoboArm %s %u", ip,
                   static_cast<unsigned>(kTcpPort));
  if (w < 0 || static_cast<size_t>(w) >= n) {
    buf[0] = '\0';
    return 0;
  }
  return static_cast<size_t>(w);
}

void ClientSession::reset() {
  state_ = State::Free;
  stamp_ = 0;
}

bool ClientSession::accept(uint32_t nowMs) {
  if (state_ != State::Free) return false;
  state_ = State::Unauthed;
  stamp_ = nowMs;
  return true;
}

ClientSession::Action ClientSession::onLine(const char* line, const char* key,
                                            uint32_t nowMs) {
  switch (state_) {
    case State::Unauthed:
      stamp_ = nowMs;
      if (keyLineMatches(line, key)) {
        state_ = State::Authed;
        return Action::ReplyReadyAndAuth;
      }
      state_ = State::BadKey;
      return Action::ReplyBadKeyThenClose;
    case State::Authed:
      stamp_ = nowMs;
      return Action::Forward;
    default:
      return Action::None;
  }
}

ClientSession::Action ClientSession::poll(uint32_t nowMs) {
  if (state_ == State::Free) return Action::None;
  uint32_t elapsed = nowMs - stamp_;
  uint32_t limit =
      state_ == State::BadKey ? kBadKeyCloseDelayMs : kIdleTimeoutMs;
  if (elapsed >= limit) {
    reset();
    return Action::Close;
  }
  return Action::None;
}
