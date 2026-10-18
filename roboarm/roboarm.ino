#include "src/core/Arm.h"
#include "src/core/LineReader.h"

#if defined(ESP32)
#include <ESP32Servo.h>
#include <BluetoothSerial.h>
#include "src/boards/config_esp32.h"
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Bluetooth Classic is not enabled for this ESP32 target (use classic ESP32, e.g. esp32:esp32:esp32)"
#endif
#elif defined(ESP8266)
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include "src/core/NetCore.h"
#include "src/boards/config_esp8266.h"
#include "src/boards/wifi_secrets.h"
#else
#include <Servo.h>
#include "src/boards/config_uno.h"
#endif

class BoardServos : public ServoOut {
 public:
  void attach(uint8_t j) override { servos_[j].attach(kConfig.joints[j].pin, 500, 2400); }
  void detach(uint8_t j) override { servos_[j].detach(); }
  void write(uint8_t j, int16_t deg) override { servos_[j].write(deg); }

 private:
  Servo servos_[kJointCount];
};

class StreamReply : public ReplySink {
 public:
  explicit StreamReply(Print& out) : out_(out) {}
  void line(const char* text) override {
    out_.print(text);
    out_.print('\n');
  }

 private:
  Print& out_;
};

static BoardServos servos;
static Arm arm(kConfig, servos);

static StreamReply serialReply(Serial);
static LineReader serialReader;

#if defined(ESP32)
static BluetoothSerial SerialBT;
static StreamReply btReply(SerialBT);
static LineReader btReader;
#endif

#if defined(ESP8266)
static WiFiServer tcpServer(kTcpPort);
static WiFiUDP udp;
static WiFiClient client;
// A client that stopped reading must not stall loop(): drop a reply that does not fit the
// send buffer instead of waiting for space (setTimeout(20) below bounds any residual wait).
class ClientReply : public ReplySink {
 public:
  explicit ClientReply(WiFiClient& c) : c_(c) {}
  void line(const char* text) override {
    size_t n = strlen(text) + 1;
    if (!c_.connected() || c_.availableForWrite() < static_cast<int>(n)) return;
    c_.print(text);
    c_.print('\n');
  }

 private:
  WiFiClient& c_;
};
static ClientReply clientReply(client);
static LineReader clientReader;
static ClientSession session;
static bool wifiWasUp = false;
static bool mdnsStarted = false;

static uint32_t lastWifiBeginMs = 0;
// "K " + key must fit in one LineReader line.
static_assert(sizeof(ROBOARM_ARM_KEY) - 1 + 2 <= LineReader::kMaxLen,
              "ARM_KEY too long for LineReader");

static void dropClient() {
  client.stop(1);  // stop(0) would wait up to 300 ms for ACKs; never block
  clientReader = LineReader();
}

static void netStart() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);  // keep UDP/mDNS replies reliable
  WiFi.setAutoReconnect(true);
  WiFi.begin(ROBOARM_WIFI_SSID, ROBOARM_WIFI_PASSWORD);
  lastWifiBeginMs = millis();
  tcpServer.begin();
  tcpServer.setNoDelay(true);
}

static void handleClientLine(const char* line, uint32_t now) {
  switch (session.onLine(line, ROBOARM_ARM_KEY, now)) {
    case ClientSession::Action::Forward:
      arm.handleLine(line, clientReply);
      break;
    case ClientSession::Action::ReplyReadyAndAuth:
      clientReply.line("READY RoboArm");
      break;
    case ClientSession::Action::ReplyBadKeyThenClose:
      clientReply.line("ERR bad key");
      break;
    default:
      break;
  }
}

static void netPump(uint32_t now) {
  wl_status_t st = WiFi.status();
  bool up = st == WL_CONNECTED;
  if (up && !wifiWasUp) {
    Serial.print("WIFI ");
    Serial.print(WiFi.localIP());
    Serial.print('\n');
    if (mdnsStarted) MDNS.end();
    mdnsStarted = MDNS.begin("roboarm");
    if (mdnsStarted) MDNS.addService("roboarm", "tcp", kTcpPort);
    udp.stop();
    udp.begin(kUdpPort);
  }
  wifiWasUp = up;
  if (mdnsStarted) MDNS.update();
  if (!up) {
    if (session.active() || client) {
      dropClient();
      session.onDisconnect();
    }
    // Auto-reconnect gives up on some failures; retry without blocking.
    if (now - lastWifiBeginMs >= 15000 &&
        (st == WL_WRONG_PASSWORD || st == WL_CONNECT_FAILED ||
         st == WL_NO_SSID_AVAIL)) {
      WiFi.begin(ROBOARM_WIFI_SSID, ROBOARM_WIFI_PASSWORD);
      lastWifiBeginMs = now;
    }
    return;
  }

  // UDP discovery.
  if (udp.parsePacket() > 0) {
    char pkt[16];
    int n = udp.read(pkt, sizeof(pkt));
    if (n > 0 && isDiscoveryRequest(pkt, static_cast<size_t>(n))) {
      char reply[48];
      String ip = WiFi.localIP().toString();
      size_t len = formatDiscoveryReply(reply, sizeof(reply), ip.c_str());
      if (len > 0) {
        udp.beginPacket(udp.remoteIP(), udp.remotePort());
        udp.write(reinterpret_cast<const uint8_t*>(reply), len);
        udp.endPacket();
      }
    }
  }

  // Drop a dead or expired client first so a quick reconnect is not "busy".
  if (session.active()) {
    if (!client.connected() && client.available() <= 0) {
      dropClient();
      session.onDisconnect();
    } else if (session.poll(now) == ClientSession::Action::Close) {
      dropClient();
    }
  }

  // New TCP connections.
  WiFiClient incoming = tcpServer.accept();
  if (incoming) {
    if (session.accept(now)) {
      client = incoming;
      client.setNoDelay(true);
      client.setTimeout(20);  // ms; bounds any blocking write to a client that stopped reading
      clientReader = LineReader();
    } else {
      incoming.setTimeout(20);
      incoming.print("ERR busy\n");
      incoming.stop(1);
    }
  }

  while (session.active() && client.available() > 0) {
    int c = client.read();
    if (c < 0) break;
    LineReader::Result r = clientReader.feed(static_cast<char>(c));
    if (r == LineReader::kOverflow) {
      // Before auth an overlong line is a bad key; after auth it is just long.
      if (session.onLine("!", ROBOARM_ARM_KEY, now) ==
          ClientSession::Action::Forward)
        clientReply.line("ERR line too long");
      else
        clientReply.line("ERR bad key");
    } else if (r == LineReader::kLine) {
      handleClientLine(clientReader.line(), now);
    }
  }
}
#endif

static void pump(Stream& in, LineReader& reader, StreamReply& reply) {
  while (in.available() > 0) {
    int c = in.read();
    if (c < 0) break;
    LineReader::Result r = reader.feed(static_cast<char>(c));
    if (r == LineReader::kLine) {
      arm.handleLine(reader.line(), reply);
    } else if (r == LineReader::kOverflow) {
      reply.line("ERR line too long");
    }
  }
}

void setup() {
  Serial.begin(115200);
  serialReply.line("READY RoboArm");
  arm.handleLine("I", serialReply);
#if defined(ESP8266)
  netStart();
#endif
#if defined(ESP32)
  SerialBT.begin("RoboArm");
  btReply.line("READY RoboArm");
  arm.handleLine("I", btReply);
#endif
}

void loop() {
  pump(Serial, serialReader, serialReply);
#if defined(ESP32)
  pump(SerialBT, btReader, btReply);
#endif
#if defined(ESP8266)
  netPump(millis());
#endif
  arm.update(millis());
}
