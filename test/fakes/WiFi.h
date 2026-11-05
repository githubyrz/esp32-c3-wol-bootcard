#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string>

#define WL_CONNECTED 3
#define WIFI_STA 1
class IPAddress {
 public:
  IPAddress() : bytes_{0, 0, 0, 0} {}
  IPAddress(uint32_t value) { memcpy(bytes_, &value, 4); }
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes_{a, b, c, d} {}
  uint8_t operator[](size_t i) const { return bytes_[i]; }
  std::string toString() const {
    char text[16]; snprintf(text, sizeof(text), "%u.%u.%u.%u", bytes_[0], bytes_[1], bytes_[2], bytes_[3]);
    return text;
  }
 private: uint8_t bytes_[4];
};
struct FakeWifi {
  int state = WL_CONNECTED;
  IPAddress ip = IPAddress(10, 23, 4, 17);
  IPAddress mask = IPAddress(255, 255, 254, 0);
  int status() const { return state; }
  IPAddress localIP() const { return ip; }
  IPAddress subnetMask() const { return mask; }
  IPAddress gatewayIP() const { return IPAddress(10,23,4,1); }
  void persistent(bool) {}
  void mode(int) {}
  void setAutoReconnect(bool) {}
  void setSleep(bool) {}
  void begin(const char*, const char*) {}
  void disconnect(bool, bool) {}
};
struct FakeSerial {
  std::string text;
  void begin(int) {}
  void println(const char* s) { text += s; text += '\n'; }
  void printf(const char* format, ...) {
    char b[1024]; va_list args; va_start(args, format);
    vsnprintf(b, sizeof(b), format, args); va_end(args); text += b;
  }
};
extern FakeWifi WiFi;
extern FakeSerial Serial;
