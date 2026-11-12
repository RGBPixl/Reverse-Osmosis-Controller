#pragma once
// Die einzigen Berührungspunkte zwischen Steuerungs-Task (loop, Core 1) und Netzwerk-Task (Core 0):
//
//   Shared<SystemStatus>  Steuerung -> Netzwerk   aktueller Zustand, als Kopie unter Mutex
//   Shared<NetworkInfo>   Netzwerk  -> Steuerung  WLAN/MQTT-Zustand für Display und LEDs
//   SettingsHub           beide                   Einstellungen; Änderungen erhöhen die Version
//   RequestQueue          Netzwerk  -> Steuerung  Befehle aus MQTT und Webpanel
//
// Relais schaltet ausschließlich die Steuerung. Der Netzwerk-Task darf blockieren
// (Verbindungsaufbau), ohne dass die Steuerung davon etwas merkt.

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <stdint.h>

#include "../config/settings.h"
#include "../core/commands.h"

namespace app {

class MutexLock {
 public:
  explicit MutexLock(SemaphoreHandle_t mutex) : mutex_(mutex) { xSemaphoreTake(mutex_, portMAX_DELAY); }
  ~MutexLock() { xSemaphoreGive(mutex_); }
  MutexLock(const MutexLock&) = delete;
  MutexLock& operator=(const MutexLock&) = delete;

 private:
  SemaphoreHandle_t mutex_;
};

// Ein Wert, den zwei Tasks gemeinsam benutzen. Zugriff nur als ganze Kopie.
// begin() muss vor dem Start des Netzwerk-Tasks aufgerufen werden.
template <typename T>
class Shared {
 public:
  void begin() { mutex_ = xSemaphoreCreateMutex(); }

  T get() const {
    MutexLock lock(mutex_);
    return value_;
  }

  void set(const T& value) {
    MutexLock lock(mutex_);
    value_ = value;
  }

 private:
  SemaphoreHandle_t mutex_ = nullptr;
  T value_;
};

// Einstellungen mit Versionszähler: Die Steuerung erkennt Änderungen aus MQTT oder Webpanel
// an der Version, übernimmt sie und speichert sie im NVS.
class SettingsHub {
 public:
  void begin(const config::Settings& initial) {
    mutex_ = xSemaphoreCreateMutex();
    settings_ = initial;
  }

  config::Settings get(uint32_t* version = nullptr) const {
    MutexLock lock(mutex_);
    if (version != nullptr) *version = version_;
    return settings_;
  }

  uint32_t version() const {
    MutexLock lock(mutex_);
    return version_;
  }

  // change bekommt die Einstellungen als Referenz. Danach wird geprüft und die Version erhöht.
  template <typename F>
  void modify(F change) {
    MutexLock lock(mutex_);
    change(settings_);
    config::sanitize(settings_);
    ++version_;
  }

 private:
  SemaphoreHandle_t mutex_ = nullptr;
  config::Settings settings_;
  uint32_t version_ = 0;
};

enum class RequestType : uint8_t {
  Controller,    // command an die Ablaufsteuerung
  ResetCounter,  // Wasserzähler auf 0
};

struct Request {
  RequestType type;
  core::Command command;

  static Request controller(const core::Command& c) { return {RequestType::Controller, c}; }
  static Request resetCounter() { return {RequestType::ResetCounter, core::Command::abortFlush()}; }
};

class RequestQueue {
 public:
  void begin() { queue_ = xQueueCreate(8, sizeof(Request)); }
  bool push(const Request& request) { return xQueueSend(queue_, &request, 0) == pdTRUE; }
  bool pop(Request& request) { return xQueueReceive(queue_, &request, 0) == pdTRUE; }

 private:
  QueueHandle_t queue_ = nullptr;
};

}  // namespace app
