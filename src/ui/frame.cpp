#include "frame.h"

#include <string.h>

namespace ui {

namespace {

// Codepunkt -> LCD-Code im HD44780-Zeichensatz A00
char toLcd(uint32_t codepoint) {
  switch (codepoint) {
    case 0x00E4:  // ä
    case 0x00C4:  // Ä (kein Großbuchstabe im ROM)
      return '\xE1';
    case 0x00F6:  // ö
    case 0x00D6:  // Ö
      return '\xEF';
    case 0x00FC:  // ü
    case 0x00DC:  // Ü
      return '\xF5';
    case 0x00DF: return '\xE2';  // ß
    case 0x00B0: return '\xDF';  // °
    case 0x00B5: return '\xE4';  // µ
    case 0x2192: return '\x7E';  // →
    case 0x2190: return '\x7F';  // ←
    default: return '?';
  }
}

}  // namespace

char nextLcdChar(const char*& utf8) {
  const uint8_t first = static_cast<uint8_t>(*utf8);
  if (first == 0) {
    return 0;
  }
  if (first < 0x80) {
    ++utf8;
    return static_cast<char>(first);
  }

  uint32_t codepoint;
  int length;
  if ((first & 0xE0) == 0xC0) {
    codepoint = first & 0x1F;
    length = 2;
  } else if ((first & 0xF0) == 0xE0) {
    codepoint = first & 0x0F;
    length = 3;
  } else if ((first & 0xF8) == 0xF0) {
    codepoint = first & 0x07;
    length = 4;
  } else {
    ++utf8;  // ungültiges Startbyte
    return '?';
  }

  for (int i = 1; i < length; ++i) {
    const uint8_t next = static_cast<uint8_t>(utf8[i]);
    if ((next & 0xC0) != 0x80) {
      utf8 += i;  // abgebrochene Folge
      return '?';
    }
    codepoint = (codepoint << 6) | (next & 0x3F);
  }
  utf8 += length;
  return toLcd(codepoint);
}

size_t lcdLength(const char* utf8) {
  size_t length = 0;
  while (nextLcdChar(utf8) != 0) {
    ++length;
  }
  return length;
}

void Frame::clear() { memset(cells, ' ', sizeof(cells)); }

void Frame::clearRow(uint8_t row) {
  if (row < kRows) memset(cells[row], ' ', kCols);
}

void Frame::print(uint8_t row, uint8_t col, const char* utf8) {
  if (row >= kRows || utf8 == nullptr) {
    return;
  }
  for (char c = nextLcdChar(utf8); c != 0 && col < kCols; c = nextLcdChar(utf8)) {
    cells[row][col++] = c;
  }
}

void Frame::printRight(uint8_t row, uint8_t endCol, const char* utf8) {
  const size_t length = lcdLength(utf8);
  const uint8_t start = length >= endCol ? 0 : static_cast<uint8_t>(endCol - length);
  print(row, start, utf8);
}

void Frame::rowText(uint8_t row, char* out) const {
  memcpy(out, cells[row < kRows ? row : 0], kCols);
  out[kCols] = '\0';
}

}  // namespace ui
