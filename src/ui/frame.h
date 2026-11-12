#pragma once
// Ein Bildschirminhalt des 16x2-LCD, bereits in LCD-Zeichencodes.
// Texte werden als UTF-8 übergeben; Umlaute, ß, ° und Pfeile werden auf den
// Zeichensatz des HD44780 (ROM A00) umgesetzt. Ohne Arduino-Abhängigkeit.

#include <stddef.h>
#include <stdint.h>

namespace ui {

constexpr uint8_t kCols = 16;
constexpr uint8_t kRows = 2;

// Eigene Zeichen im LCD (Codes 1..5, Code 0 wird bewusst nicht benutzt)
constexpr char kCharWifi0 = 1;
constexpr char kCharWifi1 = 2;
constexpr char kCharWifi2 = 3;
constexpr char kCharWifi3 = 4;
constexpr char kCharWifiOff = 5;

// Liest ein Zeichen aus UTF-8 und gibt den passenden LCD-Code zurück. Unbekanntes wird '?'.
// Rückgabe 0 am Ende des Texts.
char nextLcdChar(const char*& utf8);

// Anzahl LCD-Zeichen eines UTF-8-Texts
size_t lcdLength(const char* utf8);

struct Frame {
  char cells[kRows][kCols];

  Frame() { clear(); }

  void clear();
  void clearRow(uint8_t row);

  // Schreibt Text ab Spalte col. Was über den Rand geht, wird abgeschnitten.
  void print(uint8_t row, uint8_t col, const char* utf8);

  // Ganze Zeile: Text links, Rest mit Leerzeichen
  void setLine(uint8_t row, const char* utf8) {
    clearRow(row);
    print(row, 0, utf8);
  }

  // Text rechtsbündig, sodass er direkt vor Spalte endCol endet
  void printRight(uint8_t row, uint8_t endCol, const char* utf8);

  void put(uint8_t row, uint8_t col, char code) {
    if (row < kRows && col < kCols) cells[row][col] = code;
  }

  // Zeile als nullterminierte Zeichenkette (out braucht kCols + 1 Zeichen), für Tests und Logs
  void rowText(uint8_t row, char* out) const;
};

}  // namespace ui
