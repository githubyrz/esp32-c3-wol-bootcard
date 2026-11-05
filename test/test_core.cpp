#include "WolCore.h"
#include "WolManager.h"
#include "config.example.h"
#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits>

FakeWifi WiFi;
FakeSerial Serial;
FakeUdpState udpState;
static unsigned assertions = 0, tests = 0;
#define CHECK(expr) do { ++assertions; if (!(expr)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #expr); } while (0)
using namespace wol;
Settings fixture() {
  // Synthetic host-test fixture, never used to build firmware/connect a network.
  Settings s = kSettings;
  s.ssid = "TEST_ONLY_NETWORK"; s.password = "TEST_ONLY_PASSWORD";
  s.uid = "00112233445566778899aabbccddeeff"; s.topic = "testOnly006";
  s.protocol = Protocol::Mqtt; s.targetMac = "02:11:22:33:44:55";
  return s;
}
void test(const char* name, void (*body)()) {
  body(); ++tests; printf("PASS %s\n", name);
}
void packet() {
  const uint8_t mac[6] = {2, 17, 34, 51, 68, 85};
  uint8_t bytes[104]; memset(bytes, 0x7b, sizeof(bytes));
  static_assert(kMagicPacketSize == 102, "WOL packet length"); CHECK(makeMagicPacket(mac, bytes + 1, 102));
  CHECK(bytes[0] == 0x7b && bytes[103] == 0x7b);
  for (size_t i = 1; i <= 6; ++i) CHECK(bytes[i] == 255);
  for (size_t i = 0; i < 16; ++i) CHECK(memcmp(bytes + 7 + i * 6, mac, 6) == 0);
  CHECK(!makeMagicPacket(mac, bytes, 101)); CHECK(!makeMagicPacket(nullptr, bytes, 102));
  CHECK(!makeMagicPacket(mac, nullptr, 102));
  const uint8_t bad[6] = {}; CHECK(!makeMagicPacket(bad, bytes, 102));
}
void macFormats() {
  uint8_t actual[6], expected[6] = {0x02, 0x11, 0x22, 0xab, 0xcd, 0xef};
  for (const auto s : {"02:11:22:aB:Cd:eF", "02-11-22-AB-CD-EF", "021122abcdef"}) {
    CHECK(parseMac(s, actual)); CHECK(memcmp(actual, expected, 6) == 0);
  }
}
void invalidMac() {
  for (const auto s : {"", "02:11:22:33:44", "2:11:22:33:44:55", "02:11-22:33:44:55", "02:11:22:33:44:GG", "02:11:22:33:44:55 ", "00:00:00:00:00:00", "FF:FF:FF:FF:FF:FF", "01:11:22:33:44:55", "02.11.22.33.44.55", "021122334455junk"}) {
    uint8_t mac[6] = {9,9,9,9,9,9}; CHECK(!parseMac(s, mac)); CHECK(mac[0] == 9);
  }
  uint8_t mac[6]; CHECK(!parseMac(nullptr, mac)); CHECK(!parseMac("021122334455", nullptr));
}
void ipParsing() {
  uint32_t ip; CHECK(parseIpv4("10.23.4.17", ip)); CHECK(ip == 0x0a170411);
  for (const auto s : {"", "1.2.3", "256.1.2.3", "1.2.3.4x", "+1.2.3.4", "1..3.4", "1.2.3.4.5", "0000.2.3.4"}) CHECK(!parseIpv4(s, ip));
  CHECK(!parseIpv4(nullptr, ip));
}
void broadcast() {
  uint32_t b;
  CHECK(subnetBroadcast(0x0a170411, 0xfffffe00, b)); CHECK(b == 0x0a1705ff); // /23
  CHECK(subnetBroadcast(0xac10114b, 0xfffffff0, b)); CHECK(b == 0xac10114f); // /28
  CHECK(subnetBroadcast(0xc0a8320a, 0xffffff00, b)); CHECK(b == 0xc0a832ff);
  for (const uint32_t m : {0UL, 0xfffff0f0UL, 0xffffffffUL, 0xfffffffeUL}) CHECK(!subnetBroadcast(0x0a170411, m, b));
  CHECK(!subnetBroadcast(0x0a170400, 0xfffffe00, b));
  CHECK(!subnetBroadcast(0x0a1705ff, 0xfffffe00, b));
  CHECK(!subnetBroadcast(0xe0000001, 0xffffff00, b));
  CHECK(chooseBroadcast(0x0a170411, 0xfffffe00, "", b)); CHECK(b == 0x0a1705ff);
  CHECK(chooseBroadcast(0x0a170411, 0xfffffe00, "10.23.5.255", b));
  CHECK(chooseBroadcast(0x0a170411, 0xfffffe00, "255.255.255.255", b));
  CHECK(!chooseBroadcast(0x0a170411, 0xfffffe00, "192.168.1.255", b));
  CHECK(!chooseBroadcast(0x0a170411, 0xfffffe00, "10.23.4.17", b));
}
void commands() {
  CHECK(parseCommand((const uint8_t*)"on", 2) == Command::On);
  CHECK(parseCommand((const uint8_t*)"off", 3) == Command::Off);
  for (const auto s : {"", "ON", "On", "on ", " on", "on\n", "turnon", "only", "OFF", "off\r\n"}) CHECK(parseCommand((const uint8_t*)s, strlen(s)) == Command::Invalid);
  const uint8_t embedded[] = {'o', 'n', 0}; CHECK(parseCommand(embedded, 3) == Command::Invalid);
  CHECK(parseCommand(nullptr, 2) == Command::Invalid);
}
void startupHistory() {
  CommandGate g(15000, 3000, 10000);
  CHECK(g.handle(Command::On, false, false, 0, false) == Decision::Offline);
  g.connected(100);
  CHECK(g.handle(Command::Off, false, false, 3099, false) == Decision::Guard);
  CHECK(g.handle(Command::On, false, false, 3100, false) == Decision::NeedOff);
  CHECK(g.handle(Command::Off, true, false, 3101, false) == Decision::Retained);
  CHECK(g.handle(Command::On, true, false, 3102, false) == Decision::Retained);
  CHECK(g.handle(Command::On, false, false, 3103, false) == Decision::NeedOff);
  CHECK(g.handle(Command::Off, false, false, 3200, false) == Decision::Armed);
  CHECK(g.handle(Command::On, false, false, 3300, false) == Decision::Wake);
}
void cooldown() {
  CommandGate g(15000, 3000, 10000); g.connected(0);
  CHECK(g.handle(Command::Off, false, false, 3000, false) == Decision::Armed);
  CHECK(g.handle(Command::On, false, false, 3001, false) == Decision::Wake);
  CHECK(g.handle(Command::On, false, false, 3002, false) == Decision::Cooldown);
  CHECK(g.handle(Command::Off, false, false, 18000, false) == Decision::Cooldown);
  CHECK(g.handle(Command::On, false, false, 18001, false) == Decision::NeedOff);
  CHECK(g.handle(Command::Off, false, false, 18002, false) == Decision::Armed);
  CHECK(g.handle(Command::On, false, false, 18003, false) == Decision::Wake);
}
void sessionAndReboot() {
  CommandGate g(15000, 3000, 10000); g.connected(0);
  CHECK(g.handle(Command::Off, false, false, 3000, false) == Decision::Armed);
  CHECK(g.handle(Command::On, false, false, 3001, false) == Decision::Wake);
  g.disconnected(); g.connected(4000);
  CHECK(g.handle(Command::Off, false, false, 7000, false) == Decision::Cooldown);
  CHECK(g.handle(Command::On, true, false, 20000, false) == Decision::Retained);
  CHECK(g.handle(Command::On, false, false, 20001, false) == Decision::NeedOff);
  CommandGate reboot(15000, 3000, 10000); reboot.connected(0);
  CHECK(reboot.handle(Command::On, true, false, 3000, false) == Decision::Retained);
  CHECK(reboot.handle(Command::On, false, false, 3001, false) == Decision::NeedOff);
}
void edgeCases() {
  CommandGate g(15000, 3000, 10000); g.connected(0);
  CHECK(g.handle(Command::Off, false, false, 3000, false) == Decision::Armed);
  CHECK(g.handle(Command::On, false, false, 13000, false) == Decision::ArmExpired);
  CHECK(g.handle(Command::Off, false, false, 14000, false) == Decision::Armed);
  CHECK(g.handle(Command::On, false, true, 14001, false) == Decision::Duplicate);
  CHECK(g.handle(Command::On, false, false, 14002, false) == Decision::NeedOff);
  CHECK(g.handle(Command::Off, false, false, 15000, true) == Decision::Busy);
  CHECK(g.handle(Command::On, false, false, 15001, false) == Decision::NeedOff);
  CHECK(g.handle(Command::Off, false, false, 16000, false) == Decision::Armed);
  CHECK(g.handle(Command::Invalid, false, false, 16001, false) == Decision::Invalid);
  CHECK(g.handle(Command::On, false, false, 16002, false) == Decision::NeedOff);
}
void rollover() {
  const uint32_t base = 0xfffff000;
  CommandGate g(15000, 3000, 10000); g.connected(base);
  CHECK(g.handle(Command::Off, false, false, base + 3000, false) == Decision::Armed);
  CHECK(g.handle(Command::On, false, false, base + 3100, false) == Decision::Wake);
  CHECK(g.handle(Command::Off, false, false, base + 18099, false) == Decision::Cooldown);
  CHECK(g.handle(Command::Off, false, false, base + 18100, false) == Decision::Armed);
  CHECK(g.handle(Command::On, false, false, base + 18101, false) == Decision::Wake);
  Burst b; CHECK(b.start(0xfffffff0, 2, 100)); CHECK(b.takeDue(0xfffffff0));
  CHECK(!b.takeDue(0x53)); CHECK(b.takeDue(0x54)); CHECK(!b.active());
}
void burstScheduling() {
  Burst b; CHECK(!b.start(0, 0, 100)); CHECK(b.start(0, 3, 100)); CHECK(!b.start(0, 3, 100));
  CHECK(b.takeDue(0)); CHECK(!b.takeDue(99)); CHECK(b.takeDue(100)); CHECK(b.takeDue(200));
  CHECK(!b.takeDue(201)); CHECK(!b.active()); CHECK(b.start(500, 3, 100)); b.cancel(); CHECK(!b.takeDue(500));
}
void configChecks() {
  CHECK(validate(kSettings) != nullptr);
  Settings s = fixture(); CHECK(validate(s) == nullptr);
  s.ssid = ""; CHECK(strcmp(validate(s), "wifi_ssid") == 0); s = fixture();
  s.password = "short"; CHECK(strcmp(validate(s), "wifi_password") == 0); s = fixture();
  s.uid = "REPLACE_BEMFA_UID"; CHECK(strcmp(validate(s), "bemfa_uid") == 0); s = fixture();
  s.uid = "z0112233445566778899aabbccddeeff"; CHECK(validate(s) != nullptr); s = fixture();
  s.topic = "test/#"; CHECK(validate(s) != nullptr); s = fixture();
  s.protocol = Protocol::Unconfirmed; CHECK(validate(s) != nullptr); s.protocol = Protocol::Tcp; CHECK(validate(s) != nullptr); s = fixture();
  s.transport = Transport::PlaintextDevelopment; CHECK(validate(s) != nullptr); s.allowPlaintext = true; CHECK(validate(s) == nullptr); s = fixture();
  s.rootCa = ""; CHECK(strcmp(validate(s), "tls_root_ca") == 0); s = fixture();
  s.targetMac = "bad"; CHECK(validate(s) != nullptr); s = fixture();
  s.udpPort = 0; CHECK(validate(s) != nullptr); s = fixture();
  s.repeats = 11; CHECK(validate(s) != nullptr); s = fixture();
  s.repeatIntervalMs = 0; CHECK(validate(s) != nullptr); s = fixture();
  s.cooldownMs = 0; CHECK(validate(s) != nullptr); s = fixture();
  s.reconnectGuardMs = 0; CHECK(validate(s) != nullptr); s = fixture();
  s.armWindowMs = 0; CHECK(validate(s) != nullptr);
}
void wireFlags() {
  for (uint8_t h : {uint8_t(0x30), uint8_t(0x31), uint8_t(0x38), uint8_t(0x39)}) {
    MqttFrameTracker f; f.reset(); CHECK(f.feed(h)); CHECK(f.feed(8));
    CHECK(f.publish()); CHECK(f.retained() == bool(h & 1)); CHECK(f.duplicate() == bool(h & 8));
    for (uint8_t b : {uint8_t(0),uint8_t(3),uint8_t('a'),uint8_t('b'),uint8_t('c'),uint8_t('o'),uint8_t('f'),uint8_t('f')}) CHECK(f.feed(b));
    CHECK(f.feed(0xd0)); CHECK(f.feed(0)); CHECK(!f.publish()); // PINGRESP
    CHECK(f.feed(0x30)); CHECK(f.feed(0)); CHECK(f.publish()); CHECK(!f.duplicate()); CHECK(!f.retained());
  }
}
void frameLimits() {
  MqttFrameTracker f; CHECK(f.feed(0x30)); CHECK(f.feed(0x80)); CHECK(f.feed(0x04));
  for (int i = 0; i < 512; ++i) CHECK(f.feed(0)); // multi-byte remaining length
  CHECK(f.feed(0x31)); CHECK(f.feed(2)); CHECK(f.retained());
  f.reset(); CHECK(f.feed(0x30)); CHECK(f.feed(0x81)); CHECK(!f.feed(0x04)); CHECK(!f.feed(0));
  f.reset(); CHECK(f.feed(0x30)); CHECK(f.feed(0x80)); CHECK(f.feed(0x80)); CHECK(f.feed(0x80)); CHECK(!f.feed(0x80));
}
void replayLimit() {
  // A whole old non-retained off/on pair has no nonce/time: it is indistinguishable.
  // This executable limitation prevents documentation claiming full anti-replay.
  CommandGate g(15000, 3000, 10000); g.connected(0);
  CHECK(g.handle(Command::Off, false, false, 3000, false) == Decision::Armed);
  CHECK(g.handle(Command::On, false, false, 3001, false) == Decision::Wake);
}
void resetNetwork() { WiFi = FakeWifi(); udpState = FakeUdpState(); }
void udpIntegration() {
  resetNetwork(); Settings s = fixture(); WolManager w(s); CHECK(w.begin()); CHECK(w.start(0)); CHECK(!w.start(0));
  w.tick(0); CHECK(udpState.ends == 1); CHECK(udpState.port == 9);
  CHECK(udpState.destination.toString() == "10.23.5.255"); CHECK(udpState.payload.size() == 102);
  for (size_t i = 0; i < 6; ++i) CHECK(udpState.payload[i] == 255);
  for (size_t i = 1; i < 16; ++i) CHECK(memcmp(udpState.payload.data() + 6, udpState.payload.data() + 6 + i * 6, 6) == 0);
  w.tick(99); CHECK(udpState.ends == 1); w.tick(100); w.tick(200); CHECK(udpState.ends == 3); CHECK(!w.busy());
}
void offlineAndSubnet() {
  resetNetwork(); Settings s = fixture(); WolManager w(s); CHECK(w.begin()); WiFi.state = 0; CHECK(!w.start(0)); CHECK(udpState.starts == 0);
  WiFi.state = WL_CONNECTED; CHECK(w.start(10)); w.tick(10); WiFi.state = 0; w.tick(110); CHECK(!w.busy()); CHECK(udpState.starts == 1);
  WiFi.state = WL_CONNECTED; w.tick(210); CHECK(udpState.starts == 1); // No reconnect replay.
  CHECK(w.start(1000)); WiFi.ip = IPAddress(192,168,7,9); w.tick(1000); CHECK(!w.busy()); CHECK(udpState.starts == 1);
  s.broadcast = "192.168.1.255"; WolManager bad(s); CHECK(bad.begin()); CHECK(!bad.start(0));
}
void udpFailures() {
  for (int kind = 0; kind < 3; ++kind) {
    resetNetwork(); Settings s = fixture(); WolManager w(s); CHECK(w.begin()); CHECK(w.start(0));
    if (kind == 0) udpState.failBegin = true;
    if (kind == 1) udpState.shortWrite = true;
    if (kind == 2) udpState.failEnd = true;
    w.tick(0); w.tick(100); w.tick(200); CHECK(!w.busy()); CHECK(udpState.starts == 3);
    CHECK(udpState.ends == (kind == 2 ? 3 : 0)); CHECK(udpState.stops >= 3);
  }
}
int main(int argc, char** argv) {
  try {
    if (argc == 3 && strcmp(argv[1], "--packet") == 0) {
      uint8_t mac[6], data[kMagicPacketSize]; CHECK(parseMac("02:11:22:33:44:55", mac)); CHECK(makeMagicPacket(mac, data, sizeof(data)));
      FILE* f = nullptr; CHECK(fopen_s(&f, argv[2], "wb") == 0); CHECK(fwrite(data, 1, sizeof(data), f) == sizeof(data)); fclose(f); return 0;
    }
    test("magic_packet_layout_and_bounds", packet);
    test("mac_supported_formats", macFormats); test("mac_invalid_no_partial_output", invalidMac);
    test("ipv4_strict_parsing", ipParsing); test("non_slash24_broadcast_and_override", broadcast);
    test("exact_binary_safe_command_dispatch", commands); test("startup_guard_and_retained_history", startupHistory);
    test("cooldown_boundary_and_rearm", cooldown); test("reconnect_and_reboot_disarm", sessionAndReboot);
    test("arm_expiry_dup_busy_invalid", edgeCases); test("millis_rollover", rollover);
    test("nonblocking_burst_scheduler", burstScheduling); test("config_safe_failure_tls_opt_in", configChecks);
    test("mqtt_wire_retain_dup_flags_fragmentation", wireFlags); test("mqtt_frame_length_limits", frameLimits);
    test("documented_plain_on_off_replay_limit", replayLimit);
    test("real_wol_manager_fake_udp_three_sends", udpIntegration); test("offline_and_subnet_change_no_replay", offlineAndSubnet);
    test("udp_begin_write_end_failure_paths", udpFailures);
    printf("RESULT: %u tests, %u assertions, 0 failures\n", tests, assertions); return 0;
  } catch (const std::exception& e) { fprintf(stderr, "FAIL %s\n", e.what()); return 1; }
}
