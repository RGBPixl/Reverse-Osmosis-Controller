#include "display.h"

#include <Wire.h>

#include "../config/pins.h"
#include "../util/log.h"

namespace ui {

namespace {

// WLAN-Symbole (aus der alten Firmware übernommen)
uint8_t kWifi0[8] = {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000};
uint8_t kWifi1[8] = {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00100, 0b00000};
uint8_t kWifi2[8] = {0b00000, 0b00000, 0b00000, 0b00100, 0b01010, 0b00000, 0b00100, 0b00000};
uint8_t kWifi3[8] = {0b00100, 0b01010, 0b10001, 0b00100, 0b01010, 0b00000, 0b00100, 0b00000};
uint8_t kWifiOff[8] = {0b00100, 0b01110, 0b10101, 0b00100, 0b01110, 0b00100, 0b00100, 0b00100};

}  // namespace

Display::Display() : lcd_(pins::kLcdAddress, kCols, kRows) {}

bool Display::begin() {
  Wire.begin(pins::kI2cSda, pins::kI2cScl);
  Wire.beginTransmission(pins::kLcdAddress);
  present_ = Wire.endTransmission() == 0;
  if (!present_) {
    util::logf("Display", "Kein LCD an Adresse 0x%02X gefunden", pins::kLcdAddress);
    return false;
  }

  lcd_.init();
  lcd_.backlight();
  lcd_.createChar(kCharWifi0, kWifi0);
  lcd_.createChar(kCharWifi1, kWifi1);
  lcd_.createChar(kCharWifi2, kWifi2);
  lcd_.createChar(kCharWifi3, kWifi3);
  lcd_.createChar(kCharWifiOff, kWifiOff);
  lcd_.clear();

  shown_.clear();  // entspricht dem gelöschten LCD
  util::logf("Display", "LCD bereit");
  return true;
}

void Display::show(const Frame& frame) {
  if (!present_) {
    return;
  }
  for (uint8_t row = 0; row < kRows; ++row) {
    uint8_t col = 0;
    while (col < kCols) {
      if (frame.cells[row][col] == shown_.cells[row][col]) {
        ++col;
        continue;
      }
      // zusammenhängende Änderung in einem Rutsch schreiben
      lcd_.setCursor(col, row);
      while (col < kCols && frame.cells[row][col] != shown_.cells[row][col]) {
        lcd_.write(static_cast<uint8_t>(frame.cells[row][col]));
        shown_.cells[row][col] = frame.cells[row][col];
        ++col;
      }
    }
  }
}

}  // namespace ui
