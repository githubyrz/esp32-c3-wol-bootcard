#pragma once
#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "WolCore.h"

namespace wol {
struct CloudSnapshot { uint32_t generation; uint32_t readyAt; bool online; };
struct CloudMessage {
  uint32_t generation, receivedAt;
  uint8_t payload[16];
  uint8_t length;
  bool retained, duplicate;
};
// Only this adapter sees decrypted MQTT bytes. It never logs bytes or UID.
class ObservedClient : public Client {
 public:
  void use(WiFiClient& client) { client_ = &client; }
  int connect(IPAddress ip, uint16_t port) override;
  int connect(const char* host, uint16_t port) override;
  size_t write(uint8_t b) override { return client_->write(b); }
  size_t write(const uint8_t* b, size_t n) override { return client_->write(b, n); }
  int available() override { return client_->available(); }
  int read() override;
  int read(uint8_t* b, size_t n) override;
  int peek() override { return client_->peek(); }
  void flush() override { client_->flush(); }
  void stop() override { client_->stop(); }
  uint8_t connected() override { return client_->connected(); }
  operator bool() override { return client_ != nullptr; }
  const MqttFrameTracker& frame() const { return frame_; }
 private:
  WiFiClient* client_ = nullptr;
  MqttFrameTracker frame_;
};

// Queue/snapshot boundary is independent of MQTT, allowing later TCP adaptation.
class CloudClient {
 public:
  explicit CloudClient(const Settings& s);
  bool begin();
  CloudSnapshot snapshot();
  bool receive(CloudMessage& message);
  void requestPublishOff();
 private:
#ifdef WOL_HOST_TEST
  friend struct CloudClientTestAccess;
#endif
  static void taskEntry(void* self);
  static void messageCallback(int size);
  void run();
  void initializeTransport();
  void tick();
  void onMessage(int size);
  void setState(bool online);
  void close();
  const Settings& settings_;
  WiFiClient plain_;
  WiFiClientSecure secure_;
  ObservedClient transport_;
  MqttClient mqtt_;
  QueueHandle_t messages_ = nullptr;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  CloudSnapshot state_ = {0, 0, false};
  bool subscribed_ = false, abortPending_ = false, timeStarted_ = false;
  volatile bool publishOffPending_ = false;
  uint32_t attemptAt_ = 0, retryMs_ = 5000, timeLogAt_ = 0;
  static CloudClient* instance_;
};
}
