#pragma once
#include <stddef.h>
#include <stdint.h>

namespace wol {
constexpr size_t kMagicPacketSize = 102;
enum class Protocol { Unconfirmed, Mqtt, Tcp };
enum class Transport { Tls, PlaintextDevelopment };
struct Settings {
  const char* ssid;
  const char* password;
  const char* uid;
  const char* topic;
  Protocol protocol;
  Transport transport;
  bool allowPlaintext;
  const char* rootCa;
  const char* targetMac;
  const char* broadcast;  // Empty: derive from current DHCP IP and netmask.
  uint16_t udpPort;
  uint8_t repeats;
  uint32_t repeatIntervalMs;
  uint32_t cooldownMs;
  uint32_t reconnectGuardMs;
  uint32_t armWindowMs;
};
const char* validate(const Settings& s);  // Field names only; never returns secrets.
bool parseMac(const char* text, uint8_t out[6]);
bool makeMagicPacket(const uint8_t mac[6], uint8_t* out, size_t capacity);
bool parseIpv4(const char* text, uint32_t& out);  // Network order numeric value.
bool subnetBroadcast(uint32_t ip, uint32_t mask, uint32_t& out);
bool chooseBroadcast(uint32_t ip, uint32_t mask, const char* overrideIp, uint32_t& out);
bool elapsed(uint32_t now, uint32_t since, uint32_t interval);

enum class Command { On, Off, Update, Invalid };
Command parseCommand(const uint8_t* data, size_t length);
enum class Decision {
  Wake, Armed, Invalid, Offline, Retained, Duplicate, Guard,
  Cooldown, NeedOff, ArmExpired, Busy, Ota
};
const char* decisionText(Decision d);
class CommandGate {
 public:
  CommandGate(uint32_t cooldown, uint32_t guard, uint32_t armWindow);
  void connected(uint32_t now);
  void disconnected();
  Decision handle(Command command, bool retained, bool duplicate, uint32_t now, bool busy);
 private:
  uint32_t cooldown_, guard_, armWindow_;
  uint32_t connectedAt_ = 0, armedAt_ = 0, lastWake_ = 0;
  bool online_ = false, armed_ = false, hasWoken_ = false;
};

class Burst {
 public:
  bool start(uint32_t now, uint8_t count, uint32_t interval);
  bool takeDue(uint32_t now);
  void cancel();
  bool active() const { return remaining_ != 0; }
 private:
  uint8_t remaining_ = 0;
  uint32_t last_ = 0, interval_ = 0;
  bool first_ = false;
};

// Observe wire flags before the MQTT parser consumes them. No payload storage.
// ArduinoMqttClient 0.1.8 misreads DUP as 0x80 instead of 0x08; do not rely on it.
class MqttFrameTracker {
 public:
  void reset();
  bool feed(uint8_t byte);
  bool retained() const { return (header_ & 0x01) != 0; }
  bool duplicate() const { return (header_ & 0x08) != 0; }
  bool publish() const { return (header_ >> 4) == 3 && !fault_; }
 private:
  enum class State { Header, Length, Body } state_ = State::Header;
  uint8_t header_ = 0, lengthBytes_ = 0;
  uint32_t remaining_ = 0, multiplier_ = 1;
  bool fault_ = false;
};
}
