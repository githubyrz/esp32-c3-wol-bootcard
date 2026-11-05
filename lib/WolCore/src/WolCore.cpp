#include "WolCore.h"
#include <string.h>

namespace wol {
namespace {
int hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
bool usableMac(const uint8_t* mac) {
  uint8_t any = 0;
  for (size_t i = 0; i < 6; ++i) any |= mac[i];
  return any != 0 && !(mac[0] & 1); // Reject multicast, broadcast and zero.
}
bool placeholder(const char* s) {
  return !s || !*s || strstr(s, "REPLACE_") || strstr(s, "YOUR_");
}
}
bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}
bool parseMac(const char* text, uint8_t out[6]) {
  if (!text || !out) return false;
  const size_t n = strlen(text);
  if (n != 12 && n != 17) return false;
  const char sep = n == 17 ? text[2] : '\0';
  if (n == 17 && sep != ':' && sep != '-') return false;
  uint8_t candidate[6];
  for (size_t i = 0; i < 6; ++i) {
    const size_t p = i * (n == 17 ? 3 : 2);
    const int a = hex(text[p]), b = hex(text[p + 1]);
    if (a < 0 || b < 0 || (n == 17 && i < 5 && text[p + 2] != sep)) return false;
    candidate[i] = static_cast<uint8_t>((a << 4) | b);
  }
  if (!usableMac(candidate)) return false;
  memcpy(out, candidate, 6);
  return true;
}
bool makeMagicPacket(const uint8_t mac[6], uint8_t* out, size_t capacity) {
  if (!mac || !out || capacity < kMagicPacketSize || !usableMac(mac)) return false;
  memset(out, 0xff, 6);
  for (size_t i = 0; i < 16; ++i) memcpy(out + 6 + i * 6, mac, 6);
  return true;
}
bool parseIpv4(const char* text, uint32_t& out) {
  if (!text || !*text) return false;
  uint32_t candidate = 0;
  const char* p = text;
  for (unsigned i = 0; i < 4; ++i) {
    unsigned value = 0, digits = 0;
    while (*p >= '0' && *p <= '9') {
      value = value * 10 + static_cast<unsigned>(*p++ - '0');
      if (++digits > 3 || value > 255) return false;
    }
    if (!digits || (i < 3 ? *p != '.' : *p != '\0')) return false;
    candidate = (candidate << 8) | value;
    if (i < 3) ++p;
  }
  out = candidate;
  return true;
}
bool subnetBroadcast(uint32_t ip, uint32_t mask, uint32_t& out) {
  const uint32_t host = ~mask;
  if (!mask || host < 3 || (host & (host + 1)) != 0) return false;
  const uint8_t first = static_cast<uint8_t>(ip >> 24);
  if (first == 0 || first == 127 || first >= 224) return false;
  if ((ip & host) == 0 || (ip & host) == host) return false;
  out = ip | host;
  return true;
}
bool chooseBroadcast(uint32_t ip, uint32_t mask, const char* overrideIp, uint32_t& out) {
  uint32_t computed;
  if (!subnetBroadcast(ip, mask, computed)) return false;
  if (!overrideIp || !*overrideIp) { out = computed; return true; }
  uint32_t manual;
  if (!parseIpv4(overrideIp, manual) || (manual != computed && manual != 0xffffffffUL)) return false;
  out = manual;
  return true;
}
const char* validate(const Settings& s) {
  if (placeholder(s.ssid) || strlen(s.ssid) > 32) return "wifi_ssid";
  if (placeholder(s.password) || strlen(s.password) < 8 || strlen(s.password) > 63) return "wifi_password";
  if (placeholder(s.uid) || strlen(s.uid) != 32) return "bemfa_uid";
  for (const char* p = s.uid; *p; ++p) if (hex(*p) < 0) return "bemfa_uid";
  if (placeholder(s.topic) || strlen(s.topic) > 64) return "bemfa_topic";
  for (const char* p = s.topic; *p; ++p) {
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
          (*p >= '0' && *p <= '9') || *p == '_')) return "bemfa_topic";
  }
  if (s.protocol != Protocol::Mqtt) return "topic_protocol_must_be_confirmed_mqtt";
  if (s.transport != Transport::Tls && s.transport != Transport::PlaintextDevelopment) return "transport";
  if (s.transport == Transport::Tls && (!s.rootCa || !strstr(s.rootCa, "-----BEGIN CERTIFICATE-----"))) return "tls_root_ca";
  if (s.transport == Transport::PlaintextDevelopment && !s.allowPlaintext) return "plaintext_requires_explicit_opt_in";
  uint8_t mac[6];
  if (!parseMac(s.targetMac, mac)) return "target_mac";
  uint32_t ip;
  if (s.broadcast && *s.broadcast && !parseIpv4(s.broadcast, ip)) return "broadcast_ipv4";
  if (!s.udpPort || !s.repeats || s.repeats > 10) return "udp_port_or_repeat_count";
  if (s.repeatIntervalMs < 10 || s.repeatIntervalMs > 1000) return "repeat_interval_10_to_1000ms";
  if (s.cooldownMs < 1000 || s.cooldownMs > 3600000) return "cooldown_1s_to_1h";
  if (s.reconnectGuardMs < 1000 || s.reconnectGuardMs > 60000) return "reconnect_guard_1s_to_60s";
  if (s.armWindowMs < 1000 || s.armWindowMs > 60000) return "arm_window_1s_to_60s";
  return nullptr;
}
Command parseCommand(const uint8_t* data, size_t length) {
  if (!data) return Command::Invalid;
  if (length == 2 && data[0] == 'o' && data[1] == 'n') return Command::On;
  if (length == 3 && data[0] == 'o' && data[1] == 'f' && data[2] == 'f') return Command::Off;
  if (length == 6 && data[0] == 'u' && data[1] == 'p' && data[2] == 'd' && data[3] == 'a' && data[4] == 't' && data[5] == 'e') return Command::Update;
  if (length == 3 && data[0] == 'o' && data[1] == 't' && data[2] == 'a') return Command::Update;
  return Command::Invalid;
}
CommandGate::CommandGate(uint32_t cooldown, uint32_t guard, uint32_t armWindow)
    : cooldown_(cooldown), guard_(guard), armWindow_(armWindow) {}
void CommandGate::connected(uint32_t now) {
  online_ = true; armed_ = false; connectedAt_ = now;
}
void CommandGate::disconnected() { online_ = false; armed_ = false; }
Decision CommandGate::handle(Command c, bool retained, bool duplicate, uint32_t now, bool busy) {
  if (!online_) return Decision::Offline;
  if (retained) return Decision::Retained;
  if (duplicate) return Decision::Duplicate;
  if (c == Command::Invalid) return Decision::Invalid;
  if (!elapsed(now, connectedAt_, guard_)) return Decision::Guard;
  if (busy) return Decision::Busy;
  if (hasWoken_ && !elapsed(now, lastWake_, cooldown_)) return Decision::Cooldown;
  if (c == Command::Off) return Decision::Armed;
  if (c == Command::Update) return Decision::Ota;
  if (c == Command::On) {
    lastWake_ = now;
    hasWoken_ = true;
    return Decision::Wake;
  }
  return Decision::Invalid;
}
const char* decisionText(Decision d) {
  switch (d) {
    case Decision::Wake: return "on accepted; queue WOL (not PC status)";
    case Decision::Ota: return "update command received; start OTA";
    case Decision::Armed: return "off: no shutdown; armed for next on";
    case Decision::Invalid: return "ignored: exact on/off only";
    case Decision::Offline: return "ignored: cloud offline";
    case Decision::Retained: return "ignored: retained state/history";
    case Decision::Duplicate: return "ignored: MQTT DUP";
    case Decision::Guard: return "ignored: startup/reconnect guard";
    case Decision::Cooldown: return "ignored: cooldown; send fresh off then on later";
    case Decision::NeedOff: return "ignored: fresh off required before on";
    case Decision::ArmExpired: return "ignored: off-to-on window expired";
    case Decision::Busy: return "ignored: WOL burst busy";
  }
  return "ignored: unknown";
}
bool Burst::start(uint32_t now, uint8_t count, uint32_t interval) {
  if (active() || !count) return false;
  remaining_ = count; interval_ = interval; last_ = now; first_ = true;
  return true;
}
bool Burst::takeDue(uint32_t now) {
  if (!active() || (!first_ && !elapsed(now, last_, interval_))) return false;
  first_ = false; last_ = now; --remaining_;
  return true;
}
void Burst::cancel() { remaining_ = 0; first_ = false; }
void MqttFrameTracker::reset() {
  state_ = State::Header; header_ = 0; lengthBytes_ = 0;
  remaining_ = 0; multiplier_ = 1; fault_ = false;
}
bool MqttFrameTracker::feed(uint8_t b) {
  if (fault_) return false;
  if (state_ == State::Header) {
    header_ = b; state_ = State::Length;
    remaining_ = 0; multiplier_ = 1; lengthBytes_ = 0;
  } else if (state_ == State::Length) {
    remaining_ += (b & 0x7f) * multiplier_;
    ++lengthBytes_;
    if (remaining_ > 512 || (lengthBytes_ == 4 && (b & 0x80))) {
      fault_ = true; return false;
    }
    multiplier_ *= 128;
    if (!(b & 0x80)) state_ = remaining_ ? State::Body : State::Header;
  } else if (--remaining_ == 0) {
    state_ = State::Header;
  }
  return true;
}
}
