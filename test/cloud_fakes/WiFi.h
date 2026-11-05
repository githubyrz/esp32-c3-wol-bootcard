#pragma once
#include "Client.h"
#include <deque>
#include <functional>
struct WireState {
  std::deque<std::pair<uint32_t,uint8_t>> inbound;
  std::vector<std::vector<uint8_t>> written;
  std::function<void(const std::vector<uint8_t>&)> respond;
  bool up=false, connectOk=true, tlsCaSet=false;
  int connects=0, stops=0;
  uint16_t port=0;
  void feed(const std::vector<uint8_t>& frame, uint32_t spacing=0) {
    uint32_t when=hostMillis;
    for(auto b:frame) { inbound.emplace_back(when,b); when+=spacing; }
  }
};
extern WireState wire;
class WiFiClient : public Client {
 public:
  int connect(IPAddress, uint16_t port) override { return connect("unused", port, 3000); }
  int connect(const char* host, uint16_t port) override { return connect(host,port,3000); }
  virtual int connect(IPAddress, uint16_t port, int32_t) { return connect("unused",port,3000); }
  virtual int connect(const char*, uint16_t port, int32_t) {
    ++wire.connects; wire.port=port; wire.inbound.clear(); wire.up=wire.connectOk; return wire.up;
  }
  int setTimeout(uint32_t) { return 1; }
  size_t write(uint8_t b) override { return write(&b,1); }
  size_t write(const uint8_t* b,size_t n) override {
    if (!wire.up) return 0;
    std::vector<uint8_t> frame(b,b+n); wire.written.push_back(frame);
    if(wire.respond) wire.respond(frame);
    return n;
  }
  int available() override {
    if(!wire.up || wire.inbound.empty() || static_cast<int32_t>(hostMillis-wire.inbound.front().first)<0) return 0;
    return 1;
  }
  int read() override { if(!available()) return -1; auto b=wire.inbound.front().second;wire.inbound.pop_front();return b; }
  int read(uint8_t* b,size_t n) override { size_t i=0;for(;i<n&&available();++i)b[i]=static_cast<uint8_t>(read());return static_cast<int>(i); }
  int peek() override { return available()?wire.inbound.front().second:-1; }
  void flush() override {}
  void stop() override { wire.up=false; wire.inbound.clear(); ++wire.stops; }
  uint8_t connected() override { return wire.up; }
  operator bool() override { return true; }
};
