#pragma once
#include <stddef.h>
#include <stdint.h>

constexpr uint16_t kTcpPort = 3333;
constexpr uint16_t kUdpPort = 3334;
constexpr uint32_t kIdleTimeoutMs = 10000;
constexpr uint32_t kBadKeyCloseDelayMs = 1000;

// True iff line is exactly "K " + key. The comparison loop always covers the
// full key length (a length mismatch may return early).
bool keyLineMatches(const char* line, const char* key);

// True iff data is exactly "ROBOARM?" optionally followed by "\n" or "\r\n".
bool isDiscoveryRequest(const char* data, size_t len);

// Writes "ROBOARM RoboArm <ip> 3333" (NUL-terminated). Returns the length, or
// 0 if buf (size n) is too small.
size_t formatDiscoveryReply(char* buf, size_t n, const char* ip);

// Single-client session state. All times are millis(); wrap-safe.
class ClientSession {
 public:
  enum class Action : uint8_t {
    None,
    Forward,
    ReplyReadyAndAuth,
    ReplyBadKeyThenClose,
    Close
  };

  void reset();
  bool active() const { return state_ != State::Free; }
  // false if the slot is taken.
  bool accept(uint32_t nowMs);
  // Before auth: key check. After auth: Forward. Every line refreshes idle.
  Action onLine(const char* line, const char* key, uint32_t nowMs);
  // Close when idle >= kIdleTimeoutMs (also before auth) or, after a bad key,
  // when kBadKeyCloseDelayMs elapsed. Close also resets the session.
  Action poll(uint32_t nowMs);
  void onDisconnect() { reset(); }

 private:
  enum class State : uint8_t { Free, Unauthed, Authed, BadKey };
  State state_ = State::Free;
  uint32_t stamp_ = 0;  // last line / accept time, or bad-key time
};
