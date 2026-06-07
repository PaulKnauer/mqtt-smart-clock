#pragma once

// NetworkManager: owns WiFi management, MQTT connect/reconnect, NTP sync,
// inbound command parsing, and event publication.
//
// Cross-task rules:
// - Runs on Core 0 (NetworkTask)
// - Never accesses display or touch
// - Publishes parsed commands to commandQueue
// - Consumes events from eventQueue and publishes via MQTT

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "queue_handles.h"

class NetworkManager {
public:
  NetworkManager();
  void begin();
  void loop();

  // Public so the MQTT callback can reach it.
  static NetworkManager* instance;

private:
  // WiFi
  void ensureWiFi();
  unsigned long lastWifiAttemptMs_;

  // MQTT
  void ensureMqtt();
  void subscribeToCommands();
  void mqttCallback(char* topic, byte* payload, unsigned int length);
  static void mqttCallbackStatic(char* topic, byte* payload, unsigned int length);

  WiFiClient wifiClient_;
  PubSubClient mqttClient_;
  unsigned long lastMqttAttemptMs_;

  // NTP
  void syncNtp();
  bool ntpSynced_;
  unsigned long lastNtpAttemptMs_;

  // Event queue
  void processEventQueue();
  void publishEventToMqtt(const EventMessage& event);
  bool publishRaw(const char* topic, const char* payload, bool retained, uint8_t qos);

  // Command parsing
  void parseCommand(const char* topic, const char* jsonPayload);
  bool pushCommand(const CommandMessage& cmd);

  // Helpers
  bool isWifiConnected() const;
  unsigned long millis() const;
};
