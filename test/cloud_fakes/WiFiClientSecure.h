#pragma once
#include "WiFi.h"
class WiFiClientSecure : public WiFiClient {
 public:
  void setCACert(const char* cert) { wire.tlsCaSet=cert && strstr(cert,"BEGIN CERTIFICATE"); }
  void setHandshakeTimeout(unsigned long) {}
};
