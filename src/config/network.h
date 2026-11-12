#pragma once
// Feste Namen und Zeiten fürs Netzwerk

#include <stdint.h>

namespace config {

constexpr const char* kHostname = "ro-controller";

// WLAN-Einrichtungsportal: offenes WLAN mit diesem Namen, Seite unter http://192.168.4.1
constexpr const char* kSetupApName = "RO-Setup";
constexpr uint32_t kSetupPortalTimeoutS = 300;

constexpr const char* kNtpServer1 = "pool.ntp.org";
constexpr const char* kNtpServer2 = "time.google.com";

// MQTT: Client-ID und Topics wie in der Firmware vor dem Refactoring, damit HA nichts verliert
constexpr const char* kMqttClientId = "reverse_osmosis_controller";
constexpr const char* kMqttAvailabilityTopic = "reverse_osmosis/status";

}  // namespace config
