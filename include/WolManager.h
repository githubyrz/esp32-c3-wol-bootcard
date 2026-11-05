#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>
#include "WolCore.h"

namespace wol {
class WolManager {
 public:
  explicit WolManager(const Settings& s) : settings_(s) {}
  bool begin();
  bool start(uint32_t now);
  void tick(uint32_t now);
  bool busy() const { return burst_.active(); }
 private:
  const Settings& settings_;
  WiFiUDP udp_;
  Burst burst_;
  uint8_t packet_[kMagicPacketSize] = {};
  IPAddress broadcast_;
  uint8_t attempted_ = 0, succeeded_ = 0;
  bool ready_ = false;
};
}
