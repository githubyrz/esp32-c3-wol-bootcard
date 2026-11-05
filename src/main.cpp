#include <Arduino.h>
#include <HTTPUpdate.h>
#include <WiFiClient.h>
#include "AppConfig.h"
#include "WolWifiManager.h"
#include "CloudClient.h"
#include "WolManager.h"

namespace {
constexpr uint8_t kLedPin = 8;
constexpr uint8_t kLedOn = LOW;
constexpr uint8_t kLedOff = HIGH;

class LedController {
 public:
  void begin() {
    pinMode(kLedPin, OUTPUT);
    set(false);
  }
  void triggerWake(uint32_t now) {
    blinkCount_ = 6;
    lastToggle_ = now;
    blinkState_ = true;
    set(true);
  }
  void update(uint32_t now, bool isOnline) {
    if (blinkCount_ > 0) {
      if (now - lastToggle_ >= 80) {
        lastToggle_ = now;
        blinkState_ = !blinkState_;
        set(blinkState_);
        --blinkCount_;
      }
      return;
    }
    set(isOnline);
  }
  void set(bool on) {
    digitalWrite(kLedPin, on ? kLedOn : kLedOff);
  }
 private:
  uint8_t blinkCount_ = 0;
  uint32_t lastToggle_ = 0;
  bool blinkState_ = false;
};

LedController led;
wol::WolWifiManager wifi(wol::kSettings);
wol::CloudClient cloud(wol::kSettings);
wol::WolManager wolSender(wol::kSettings);
wol::CommandGate gate(wol::kSettings.cooldownMs, wol::kSettings.reconnectGuardMs, wol::kSettings.armWindowMs);
bool enabled = false;
uint32_t generation = 0;
bool autoResetPending = false;
uint32_t autoResetAt = 0;
void syncCloud(const wol::CloudSnapshot& s) {
  if (s.generation == generation) return;
  generation = s.generation;
  if (s.online) gate.connected(s.readyAt);
  else gate.disconnected();
}
}
void setup() {
  setCpuFrequencyMhz(80);
  led.begin();
  Serial.begin(115200);
  // Do not wait for a USB terminal: the device must work with a charger alone.
  Serial.println("\n[boot] ESP32 Bemfa WOL 1.0; PC power state UNKNOWN");
  const char* error = wol::validate(wol::kSettings);
  if (error) {
    Serial.printf("[config] SAFE STOP: check field '%s'; no Wi-Fi/cloud/WOL started\n", error);
    return;
  }
  if (!wolSender.begin() || !cloud.begin()) {
    Serial.println("[boot] SAFE STOP: WOL init or cloud task allocation failed"); return;
  }
  wifi.begin(); enabled = true;
}
void loop() {
  if (!enabled) { delay(100); return; }
  uint32_t now = millis();
  wifi.tick(now);
  syncCloud(cloud.snapshot());
  wol::CloudMessage message;
  // Bounded queue processing keeps Wi-Fi and WOL scheduling responsive.
  for (unsigned count = 0; count < 8 && cloud.receive(message); ++count) {
    const auto s = cloud.snapshot(); syncCloud(s); now = millis();
    if (!s.online || s.generation != message.generation || WiFi.status() != WL_CONNECTED ||
        wol::elapsed(now, message.receivedAt, 2000)) {
      gate.disconnected(); generation = 0;
      Serial.println("[command] ignored: offline, expired or old connection generation"); continue;
    }
    const auto command = wol::parseCommand(message.payload, message.length);
    const auto decision = gate.handle(command, message.retained, message.duplicate, message.receivedAt, wolSender.busy());
    Serial.printf("[command] %s\n", wol::decisionText(decision));
    if (decision == wol::Decision::Wake) {
      wolSender.start(now);
      led.triggerWake(now);
      autoResetPending = true;
      autoResetAt = now;
    }
    if (decision == wol::Decision::Ota) {
      Serial.println("[ota] Bemfa OTA request received!");
      led.set(true);
      String otaUrl = "http://bin.bemfa.com/b/3" + String(wol::kSettings.uid) + String(wol::kSettings.topic) + ".bin";
      Serial.printf("[ota] Downloading from: %s\n", otaUrl.c_str());
      WiFiClient otaClient;
      httpUpdate.rebootOnUpdate(true);
      t_httpUpdate_return ret = httpUpdate.update(otaClient, otaUrl);
      if (ret == HTTP_UPDATE_FAILED) {
        Serial.printf("[ota] Failed Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      } else if (ret == HTTP_UPDATE_NO_UPDATES) {
        Serial.println("[ota] No updates on server");
      } else if (ret == HTTP_UPDATE_OK) {
        Serial.println("[ota] Update succeeded, rebooting into new firmware...");
      }
    }
  }
  wolSender.tick(millis());
  if (autoResetPending && wol::elapsed(millis(), autoResetAt, 2000)) {
    autoResetPending = false;
    cloud.requestPublishOff();
  }
  const auto s = cloud.snapshot();
  led.update(millis(), s.online && WiFi.status() == WL_CONNECTED);
  delay(wolSender.busy() ? 2 : 20);
}
