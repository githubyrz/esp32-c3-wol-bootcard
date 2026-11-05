#pragma once
#include <WiFi.h>
#include "WolCore.h"

namespace wol {
class WolWifiManager {
 public:
  explicit WolWifiManager(const Settings& s) : settings_(s) {}
  void begin();
  void tick(uint32_t now);
 private:
  const Settings& settings_;
  uint32_t lastAttempt_ = 0;
  bool connected_ = false;
};
}
