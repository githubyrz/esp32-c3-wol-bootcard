#pragma once
#include <WiFi.h>
#include <vector>
struct FakeUdpState {
  int starts = 0, writes = 0, ends = 0, stops = 0;
  bool failBegin = false, shortWrite = false, failEnd = false;
  uint16_t port = 0;
  IPAddress destination;
  std::vector<uint8_t> payload;
};
extern FakeUdpState udpState;
class WiFiUDP {
 public:
  int beginPacket(IPAddress address, uint16_t port) {
    ++udpState.starts; udpState.destination = address; udpState.port = port;
    return !udpState.failBegin;
  }
  size_t write(const uint8_t* b, size_t n) {
    ++udpState.writes; udpState.payload.assign(b, b + n);
    return udpState.shortWrite ? n - 1 : n;
  }
  int endPacket() { ++udpState.ends; return !udpState.failEnd; }
  void stop() { ++udpState.stops; }
};
