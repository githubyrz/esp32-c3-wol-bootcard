#include "WolWifiManager.h"

namespace wol {
void WolWifiManager::begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // One owner of retry scheduling.
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_15dBm);
  Serial.println("[wifi] connecting (credentials hidden)");
  WiFi.begin(settings_.ssid, settings_.password);
  lastAttempt_ = millis();
}
void WolWifiManager::tick(uint32_t now) {
  const bool up = WiFi.status() == WL_CONNECTED;
  if (up != connected_) {
    connected_ = up;
    if (up) {
      Serial.printf("[wifi] connected IP=%s gateway=%s mask=%s\n",
          WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(),
          WiFi.subnetMask().toString().c_str());
    } else {
      Serial.println("[wifi] disconnected; retry in 15 seconds");
      lastAttempt_ = now;
    }
  }
  if (!up && elapsed(now, lastAttempt_, 15000)) {
    Serial.println("[wifi] retry connection");
    WiFi.disconnect(false, false);
    WiFi.begin(settings_.ssid, settings_.password);
    lastAttempt_ = now;
  }
}
}
