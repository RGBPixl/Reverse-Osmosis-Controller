#pragma once
// Startseite und Menü des LCD. Zeichnet nur in einen Frame und schickt Befehle an einen MenuHost,
// kennt also weder LCD noch Relais. Ohne Arduino-Abhängigkeit, mit Unit-Tests (test/test_core).
//
// Bedienung:
//   Startseite:  OK = Menü öffnen (bei Fehler direkt zu "Fehler quittieren")
//   Liste:       Rechts = weiter, Links = zurück, OK = öffnen / ausführen
//   Links lang:  zurück zur Startseite
//   30 s ohne Taste: zurück zur Startseite

#include <stddef.h>
#include <stdint.h>

#include "../core/commands.h"
#include "../core/status.h"
#include "frame.h"

namespace ui {

enum class Key : uint8_t { Ok, Left, Right };
enum class Press : uint8_t { Short, Long };

// Was das Menü auslösen kann. Implementiert von der App.
class MenuHost {
 public:
  virtual ~MenuHost() {}
  virtual bool sendCommand(const core::Command& command) = 0;
  virtual void resetWaterCounter() = 0;
  virtual void factoryReset() = 0;
  virtual void startLedTest() = 0;
};

class Menu {
 public:
  static constexpr uint32_t kIdleTimeoutMs = 30000;
  static constexpr uint32_t kConfirmTimeoutMs = 5000;
  static constexpr uint32_t kMessageMs = 2000;
  static constexpr uint32_t kAlternateMs = 3000;  // Wechsel zwischen zwei Infos in einer Zeile

  explicit Menu(MenuHost& host) : host_(host) {}

  void handleKey(Key key, Press press, const core::SystemStatus& status, uint32_t nowMs);

  // Zeichnet den aktuellen Bildschirm. Spalte 15 der ersten Zeile bleibt auf der Startseite
  // frei für das WLAN-Symbol.
  void render(const core::SystemStatus& status, uint32_t nowMs, Frame& frame);

  bool atHome() const { return level_ == Level::Home; }

 private:
  enum class Level : uint8_t { Home, Categories, Items };

  void goHome();
  void openCategory(uint8_t category, const core::SystemStatus& status);
  bool itemVisible(uint8_t category, uint8_t item, const core::SystemStatus& status) const;
  void moveToVisibleItem(const core::SystemStatus& status, bool forward);
  void activateItem(const core::SystemStatus& status, uint32_t nowMs);
  void showMessage(const char* text, uint32_t nowMs);

  void renderHome(const core::SystemStatus& status, uint32_t nowMs, Frame& frame) const;
  void renderCategories(Frame& frame) const;
  void renderItem(const core::SystemStatus& status, uint32_t nowMs, Frame& frame) const;

  MenuHost& host_;
  Level level_ = Level::Home;
  uint8_t category_ = 0;
  uint8_t item_ = 0;
  uint32_t lastKeyMs_ = 0;

  bool confirmPending_ = false;
  uint32_t confirmSinceMs_ = 0;

  const char* message_ = nullptr;
  uint32_t messageUntilMs_ = 0;
};

}  // namespace ui
