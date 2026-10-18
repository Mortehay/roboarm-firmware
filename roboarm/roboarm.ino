#include "src/core/Arm.h"
#include "src/core/LineReader.h"

#if defined(ESP32)
#include <ESP32Servo.h>
#include <BluetoothSerial.h>
#include "src/boards/config_esp32.h"
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Bluetooth Classic is not enabled for this ESP32 target (use classic ESP32, e.g. esp32:esp32:esp32)"
#endif
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
  arm.update(millis());
}
