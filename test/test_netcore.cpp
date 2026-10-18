#include <string.h>
#include "core/NetCore.h"
#include "check.h"

using A = ClientSession::Action;

TEST(net_key_match) {
  CHECK(keyLineMatches("K abcdefgh", "abcdefgh"));
  CHECK(!keyLineMatches("K abcdefg", "abcdefgh"));
  CHECK(!keyLineMatches("K abcdefghi", "abcdefgh"));
  CHECK(!keyLineMatches("k abcdefgh", "abcdefgh"));
  CHECK(!keyLineMatches("Kabcdefgh", "abcdefgh"));
  CHECK(!keyLineMatches("", "abcdefgh"));
  CHECK(!keyLineMatches("K abcdefgX", "abcdefgh"));
  CHECK(!keyLineMatches("K ", ""));
  CHECK(!keyLineMatches("K", ""));
}
TEST(net_discovery_request) {
  CHECK(isDiscoveryRequest("ROBOARM?", 8));
  CHECK(isDiscoveryRequest("ROBOARM?\n", 9));
  CHECK(isDiscoveryRequest("ROBOARM?\r\n", 10));
  CHECK(!isDiscoveryRequest("ROBOARM", 7));
  CHECK(!isDiscoveryRequest("roboarm?", 8));
  CHECK(!isDiscoveryRequest("ROBOARM?x", 9));
}
TEST(net_discovery_reply) {
  char buf[64];
  size_t n = formatDiscoveryReply(buf, sizeof buf, "192.168.1.42");
  CHECK_STR(buf, "ROBOARM RoboArm 192.168.1.42 3333");
  CHECK_EQ(n, strlen("ROBOARM RoboArm 192.168.1.42 3333"));
  char small[10];
  CHECK_EQ(formatDiscoveryReply(small, sizeof small, "192.168.1.42"), (size_t)0);
}
TEST(net_busy) {
  ClientSession s;
  CHECK(!s.active());
  CHECK(s.accept(0));
  CHECK(s.active());
  CHECK(!s.accept(1));
}
TEST(net_bad_key) {
  ClientSession s;
  s.accept(0);
  CHECK(s.onLine("?", "k1234567", 5) == A::ReplyBadKeyThenClose);
  CHECK(s.onLine("K k1234567", "k1234567", 6) == A::None);
  CHECK(s.poll(500) == A::None);
  CHECK(s.poll(1004) == A::None);
  CHECK(s.poll(1005) == A::Close);
  CHECK(!s.active());
  CHECK(s.accept(2000));
}
TEST(net_good_key_and_idle) {
  ClientSession s;
  s.accept(0);
  CHECK(s.onLine("K k1234567", "k1234567", 100) == A::ReplyReadyAndAuth);
  CHECK(s.onLine("?", "k1234567", 200) == A::Forward);
  CHECK(s.poll(200 + 9999) == A::None);
  CHECK(s.onLine("?", "k1234567", 5000) == A::Forward);  // refresh
  CHECK(s.poll(5000 + 9999) == A::None);
  CHECK(s.poll(5000 + 10000) == A::Close);
  CHECK(!s.active());
}
TEST(net_auth_timeout_before_key) {
  ClientSession s;
  s.accept(1000);
  CHECK(s.poll(1000 + kAuthTimeoutMs - 1) == A::None);
  CHECK(s.poll(1000 + kAuthTimeoutMs) == A::Close);
  CHECK(!s.active());
  CHECK(kAuthTimeoutMs == 3000);
}
TEST(net_auth_timeout_millis_wrap) {
  ClientSession s;
  s.accept(0xFFFFF800u);
  CHECK(s.poll(0xFFFFF800u + kAuthTimeoutMs - 1) == A::None);  // wraps past 0
  CHECK(s.poll(0xFFFFF800u + kAuthTimeoutMs) == A::Close);
}
TEST(net_idle_limit_after_auth_is_10s) {
  ClientSession s;
  s.accept(0);
  CHECK(s.onLine("K k1234567", "k1234567", 100) == A::ReplyReadyAndAuth);
  CHECK(s.poll(100 + kAuthTimeoutMs) == A::None);  // 3 s limit no longer applies
  CHECK(s.poll(100 + kIdleTimeoutMs - 1) == A::None);
  CHECK(s.poll(100 + kIdleTimeoutMs) == A::Close);
}
TEST(net_disconnect_frees) {
  ClientSession s;
  s.accept(0);
  s.onDisconnect();
  CHECK(!s.active());
  CHECK(s.accept(1));
}
TEST(net_no_client) {
  ClientSession s;
  CHECK(s.onLine("?", "k1234567", 0) == A::None);
  CHECK(s.poll(99999) == A::None);
}
TEST(net_millis_wrap) {
  ClientSession s;
  s.accept(0xFFFFF000u);
  CHECK(s.onLine("K k1234567", "k1234567", 0xFFFFF000u) == A::ReplyReadyAndAuth);
  CHECK(s.poll(0xFFFFF000u + 9999) == A::None);
  CHECK(s.poll(0x00001000u + 10000) == A::Close);
}
