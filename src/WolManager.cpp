#include "WolManager.h"

namespace wol {
namespace {
uint32_t networkNumber(const IPAddress& a) {
  return (uint32_t(a[0]) << 24) | (uint32_t(a[1]) << 16) | (uint32_t(a[2]) << 8) | a[3];
}
}
bool WolManager::begin() {
  uint8_t mac[6];
  ready_ = parseMac(settings_.targetMac, mac) && makeMagicPacket(mac, packet_, sizeof(packet_));
  return ready_;
}
bool WolManager::start(uint32_t now) {
  if (!ready_ || busy() || WiFi.status() != WL_CONNECTED) {
    Serial.println("[wol] refused: invalid config, Wi-Fi offline or burst busy"); return false;
  }
  uint32_t address;
  if (!chooseBroadcast(networkNumber(WiFi.localIP()), networkNumber(WiFi.subnetMask()), settings_.broadcast, address)) {
    Serial.println("[wol] refused: invalid subnet or broadcast override"); return false;
  }
  broadcast_ = IPAddress(static_cast<uint8_t>(address >> 24), static_cast<uint8_t>(address >> 16),
      static_cast<uint8_t>(address >> 8), static_cast<uint8_t>(address));
  attempted_ = succeeded_ = 0;
  Serial.printf("[wol] scheduling %u UDP packets to %s:%u\n", settings_.repeats,
      broadcast_.toString().c_str(), settings_.udpPort);
  return burst_.start(now, settings_.repeats, settings_.repeatIntervalMs);
}
void WolManager::tick(uint32_t now) {
  if (!busy()) return;
  uint32_t currentBroadcast;
  if (WiFi.status() != WL_CONNECTED ||
      !chooseBroadcast(networkNumber(WiFi.localIP()), networkNumber(WiFi.subnetMask()), settings_.broadcast, currentBroadcast) ||
      currentBroadcast != networkNumber(broadcast_)) {
    burst_.cancel(); udp_.stop();
    Serial.println("[wol] canceled: Wi-Fi/subnet changed; no queued wake replay"); return;
  }
  if (!burst_.takeDue(now)) return;
  ++attempted_;
  bool ok = udp_.beginPacket(broadcast_, settings_.udpPort) == 1;
  if (ok) ok = udp_.write(packet_, sizeof(packet_)) == sizeof(packet_);
  if (ok) ok = udp_.endPacket() == 1;
  if (ok) ++succeeded_;
  else { udp_.stop(); Serial.printf("[wol] local UDP attempt %u failed\n", attempted_); }
  if (!busy()) {
    udp_.stop();
    Serial.printf("[wol] local UDP sends completed: %u/%u; PC power state is UNKNOWN\n", succeeded_, attempted_);
  }
}
}
