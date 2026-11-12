#pragma once
// LCD 16x2 über I2C. Einziger Teil der Firmware, der aufs Display schreibt.
// show() überträgt nur Zeichen, die sich geändert haben: kein Flackern, kaum I2C-Last.

#include <LiquidCrystal_I2C.h>

#include "frame.h"

namespace ui {

class Display {
 public:
  Display();

  // Gibt false zurück, wenn unter der Adresse kein LCD antwortet. Dann bleiben alle Aufrufe wirkungslos.
  bool begin();

  void show(const Frame& frame);

  bool present() const { return present_; }

 private:
  LiquidCrystal_I2C lcd_;
  bool present_ = false;
  Frame shown_;
};

}  // namespace ui
