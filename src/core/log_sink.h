#pragma once
// Die Steuerlogik kennt kein Serial. Wer Meldungen sehen will, setzt eine Senke.

namespace core {

using LogSink = void (*)(const char* message);

}  // namespace core
