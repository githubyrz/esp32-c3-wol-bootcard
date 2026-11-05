#include "CloudClient.h"
#include <time.h>

namespace wol {
CloudClient* CloudClient::instance_ = nullptr;
int ObservedClient::connect(IPAddress ip, uint16_t port) {
  frame_.reset(); return client_->connect(ip, port, 3000);
}
int ObservedClient::connect(const char* host, uint16_t port) {
  frame_.reset(); return client_->connect(host, port, 3000);
}
int ObservedClient::read() {
  const int b = client_->read();
  if (b >= 0 && !frame_.feed(static_cast<uint8_t>(b))) { client_->stop(); return -1; }
  return b;
}
int ObservedClient::read(uint8_t* b, size_t n) {
  size_t i = 0;
  while (i < n) { const int value = read(); if (value < 0) break; b[i++] = value; }
  return static_cast<int>(i);
}
CloudClient::CloudClient(const Settings& s) : settings_(s), mqtt_(transport_) {}
bool CloudClient::begin() {
  if (instance_ || validate(settings_)) return false;
  messages_ = xQueueCreate(8, sizeof(CloudMessage));
  if (!messages_) return false;
  instance_ = this;
  const BaseType_t ok = xTaskCreate(taskEntry, "bemfa-mqtt", 8192, this, 1, nullptr);
  if (ok != pdPASS) { instance_ = nullptr; vQueueDelete(messages_); messages_ = nullptr; return false; }
  return true;
}
CloudSnapshot CloudClient::snapshot() {
  portENTER_CRITICAL(&mux_); const auto result = state_; portEXIT_CRITICAL(&mux_);
  return result;
}
bool CloudClient::receive(CloudMessage& message) { return messages_ && xQueueReceive(messages_, &message, 0) == pdTRUE; }
void CloudClient::requestPublishOff() { publishOffPending_ = true; }
void CloudClient::setState(bool online) {
  portENTER_CRITICAL(&mux_);
  ++state_.generation; state_.online = online; state_.readyAt = millis();
  portEXIT_CRITICAL(&mux_);
}
void CloudClient::close() {
  const bool wasOnline = subscribed_;
  subscribed_ = false;
  setState(false); // Invalidate queued commands BEFORE any blocking socket stop.
  mqtt_.stop();
  abortPending_ = false;
  if (wasOnline) Serial.println("[cloud] disconnected; disarmed; queued old commands invalidated");
}
void CloudClient::taskEntry(void* self) { static_cast<CloudClient*>(self)->run(); }
void CloudClient::messageCallback(int size) { if (instance_) instance_->onMessage(size); }
void CloudClient::run() {
  initializeTransport();
  for (;;) { tick(); vTaskDelay(pdMS_TO_TICKS(20)); }
}
void CloudClient::initializeTransport() {
  if (settings_.transport == Transport::Tls) {
    secure_.setCACert(settings_.rootCa);
    secure_.setHandshakeTimeout(5);
    secure_.setTimeout(2);
    transport_.use(secure_);
    Serial.println("[cloud] TLS 9503 enabled; CA + hostname required; no fallback");
  } else {
    plain_.setTimeout(2);
    transport_.use(plain_);
    Serial.println("[cloud] WARNING: explicit DEVELOPMENT plaintext 9501; UID is unencrypted");
  }
  mqtt_.setId(settings_.uid); // Bemfa UID is the authentication Client ID.
  mqtt_.setCleanSession(true);
  mqtt_.setKeepAliveInterval(30000);
  mqtt_.setConnectionTimeout(2000);
  mqtt_.setTimeout(250); // Payload is exactly 2 or 3 bytes; bounded Stream read.
  mqtt_.onMessage(messageCallback);
  attemptAt_ = millis() - retryMs_;
}
void CloudClient::tick() {
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (subscribed_) close();
    timeStarted_ = false;
    return;
  }
  if (settings_.transport == Transport::Tls && time(nullptr) < 1735689600) {
    if (!timeStarted_) {
      configTime(0, 0, "ntp.aliyun.com", "time.cloudflare.com", "pool.ntp.org");
      timeStarted_ = true;
      Serial.println("[cloud] waiting for SNTP time before certificate verification");
    }
    if (elapsed(now, timeLogAt_, 60000)) {
      Serial.println("[cloud] time unavailable; TLS remains disabled until clock is valid"); timeLogAt_ = now;
    }
    return;
  }
  if (subscribed_) {
    mqtt_.poll();
    if (publishOffPending_) {
      publishOffPending_ = false;
      mqtt_.beginMessage(settings_.topic);
      mqtt_.print("off");
      mqtt_.endMessage();
      Serial.println("[cloud] auto-reset: published 'off' to restore switch state");
    }
    if (abortPending_ || !mqtt_.connected()) { close(); attemptAt_ = millis(); }
    return;
  }
  if (!elapsed(now, attemptAt_, retryMs_)) return;
  Serial.println("[cloud] connecting to Bemfa (authentication hidden)");
  const uint16_t port = settings_.transport == Transport::Tls ? 9503 : 9501;
  bool ok = mqtt_.connect("bemfa.com", port) == 1;
  if (ok && WiFi.status() == WL_CONNECTED) {
    // Callback may run during subscribe: subscribed_ is still false, so drop it.
    ok = mqtt_.subscribe(settings_.topic, 0) == 1 && mqtt_.subscribeQoS() == 0;
  } else ok = false;
  if (ok && !abortPending_ && WiFi.status() == WL_CONNECTED) {
    subscribed_ = true; setState(true); retryMs_ = 5000;
    Serial.println("[cloud] MQTT connected; SUBACK granted QoS0; guard then fresh off -> on required");
  } else {
    Serial.printf("[cloud] connect/subscribe failed; connectError=%d; SUBACK=%d\n", mqtt_.connectError(), mqtt_.subscribeQoS());
    close(); retryMs_ = retryMs_ < 30000 ? retryMs_ * 2 : 60000;
  }
  attemptAt_ = millis();
}
void CloudClient::onMessage(int size) {
  // Capture flags BEFORE reading the payload; the parser advances after read.
  const auto& frame = transport_.frame();
  CloudMessage message = {};
  message.retained = frame.retained(); message.duplicate = frame.duplicate();
  if (!frame.publish() || size < 2 || size > 16 || mqtt_.messageQoS() != 0 ||
      mqtt_.messageTopic() != settings_.topic) {
    Serial.println("[cloud] ignored: invalid topic, size or QoS; reconnect to discard frame");
    abortPending_ = true; return;
  }
  const size_t n = mqtt_.readBytes(message.payload, static_cast<size_t>(size));
  if (n != static_cast<size_t>(size)) {
    Serial.println("[cloud] ignored: incomplete payload; reconnect"); abortPending_ = true; return;
  }
  if (!subscribed_) { Serial.println("[cloud] ignored: pre-SUBACK state/history"); return; }
  if (abortPending_) return;
  const auto state = snapshot();
  message.generation = state.generation; message.receivedAt = millis(); message.length = static_cast<uint8_t>(n);
  if (xQueueSend(messages_, &message, 0) != pdTRUE) {
    Serial.println("[cloud] command queue full; fail closed and reconnect"); abortPending_ = true;
    setState(false);
  }
}
}
