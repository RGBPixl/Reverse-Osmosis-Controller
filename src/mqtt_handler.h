#pragma once
#include <PubSubClient.h>
#include <WiFiClient.h>

void setupMqtt();
void handleMqttLoop();
void publishMqttDiscovery();
void publishMqttState();
void checkAndPublishRelayStates();
