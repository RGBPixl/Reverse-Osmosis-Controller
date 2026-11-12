#include "protocol.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace net {

namespace {

bool equalsIgnoreCase(const char* a, const char* b) {
  for (; *a != '\0' && *b != '\0'; ++a, ++b) {
    if (tolower(static_cast<unsigned char>(*a)) != tolower(static_cast<unsigned char>(*b))) {
      return false;
    }
  }
  return *a == *b;
}

// Liest eine ein- oder zweistellige Zahl und rückt den Zeiger weiter.
bool readNumber(const char*& p, int& value) {
  if (!isdigit(static_cast<unsigned char>(*p))) return false;
  value = *p++ - '0';
  if (isdigit(static_cast<unsigned char>(*p))) value = value * 10 + (*p++ - '0');
  return true;
}

const char* programLongName(core::Program p) {
  switch (p) {
    case core::Program::Startup: return "Startspülung";
    case core::Program::System: return "Systemspülung";
    case core::Program::Membrane: return "Membranspülung";
  }
  return "?";
}

}  // namespace

void copyPayload(const uint8_t* payload, size_t length, char* out, size_t outLen) {
  if (outLen == 0) return;
  size_t start = 0;
  while (start < length && isspace(payload[start])) ++start;
  size_t end = length;
  while (end > start && isspace(payload[end - 1])) --end;
  size_t n = end - start;
  if (n >= outLen) n = outLen - 1;
  memcpy(out, payload + start, n);
  out[n] = '\0';
}

bool parseOnOff(const char* text, bool& on) {
  if (equalsIgnoreCase(text, "ON") || equalsIgnoreCase(text, "1") || equalsIgnoreCase(text, "true")) {
    on = true;
    return true;
  }
  if (equalsIgnoreCase(text, "OFF") || equalsIgnoreCase(text, "0") || equalsIgnoreCase(text, "false")) {
    on = false;
    return true;
  }
  return false;
}

bool isPress(const char* text) {
  return equalsIgnoreCase(text, "PRESS") || equalsIgnoreCase(text, "1") || equalsIgnoreCase(text, "ON");
}

bool parseHours(const char* text, uint8_t maxHours, uint8_t& hours) {
  char* end = nullptr;
  const double value = strtod(text, &end);
  if (end == text || *end != '\0') return false;
  if (!(value >= 0.0 && value <= maxHours) || value != floor(value)) return false;
  hours = static_cast<uint8_t>(value);
  return true;
}

bool parseFlushTime(const char* text, config::FlushTime& time) {
  if (*text == '\0' || strcmp(text, "--:--") == 0 || equalsIgnoreCase(text, "aus") || equalsIgnoreCase(text, "off")) {
    time.enabled = false;
    return true;
  }

  const char* p = text;
  int hour = 0;
  int minute = 0;
  if (!readNumber(p, hour) || *p++ != ':') return false;
  if (!isdigit(static_cast<unsigned char>(p[0])) || !isdigit(static_cast<unsigned char>(p[1]))) return false;
  minute = (p[0] - '0') * 10 + (p[1] - '0');
  p += 2;
  if (*p == ':') {  // Sekunden werden ignoriert
    ++p;
    int seconds = 0;
    if (!readNumber(p, seconds)) return false;
  }
  if (*p != '\0' || hour > 23 || minute > 59) return false;

  time.enabled = true;
  time.hour = static_cast<uint8_t>(hour);
  time.minute = static_cast<uint8_t>(minute);
  return true;
}

void formatFlushTime(const config::FlushTime& time, char* out, size_t len) {
  if (time.enabled) {
    snprintf(out, len, "%02u:%02u", static_cast<unsigned>(time.hour), static_cast<unsigned>(time.minute));
  } else {
    snprintf(out, len, "--:--");
  }
}

void stateText(const core::ControllerStatus& status, char* out, size_t len) {
  switch (status.mode) {
    case core::Mode::Idle: snprintf(out, len, "Bereit"); return;
    case core::Mode::Producing: snprintf(out, len, "Produziert"); return;
    case core::Mode::Flushing: snprintf(out, len, "Spült: %s", programLongName(status.program)); return;
    case core::Mode::Service: snprintf(out, len, "Service"); return;
    case core::Mode::Fault: snprintf(out, len, "Fehler"); return;
  }
  snprintf(out, len, "?");
}

bool hasProblem(const core::ControllerStatus& status) {
  return status.mode == core::Mode::Fault || status.warnings != 0;
}

void problemText(const core::ControllerStatus& status, char* out, size_t len) {
  if (status.mode == core::Mode::Fault) {
    snprintf(out, len, "Fehler: %s", core::faultText(status.fault));
  } else if (status.warnings & core::kWarnValveLeak) {
    snprintf(out, len, "Warnung: Zulauf zu, aber Durchfluss (Ventil undicht?)");
  } else if (status.warnings & core::kWarnNoInletFlow) {
    snprintf(out, len, "Warnung: Zulauf offen, aber kein Durchfluss");
  } else {
    snprintf(out, len, "OK");
  }
}

}  // namespace net
