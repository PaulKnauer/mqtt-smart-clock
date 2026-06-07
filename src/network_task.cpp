#include "network_task.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>
#if __has_include("credentials.h")
#  include "credentials.h"
#else
#  error "credentials.h not found. Copy credentials.h.example to credentials.h and fill in your WiFi/MQTT settings."
#endif
#include "device_config.h"
#include "mqtt_topics.h"
#include "queue_handles.h"

// Singleton instance for the static MQTT callback.
NetworkManager* NetworkManager::instance = nullptr;

// ── Retry intervals ──────────────────────────────────────────────────────────
constexpr unsigned long kWifiRetryMs  = 5000;
constexpr unsigned long kMqttRetryMs  = 5000;
constexpr unsigned long kNtpRetryMs   = 30000;
constexpr unsigned long kEventPollMs  = 100;
constexpr int kNtpTimeoutSec          = 10;
static constexpr const char* kOfflinePayload = "offline";
static constexpr const char* kOnlinePayload  = "online";

// Minimum epoch timestamp that indicates NTP has synced.
// Anything before ~2024 (epoch 1704000000) is pre-NTP realtime-clock junk.
static constexpr time_t kMinNtpEpoch = 1704000000;

// ── Construction ─────────────────────────────────────────────────────────────

NetworkManager::NetworkManager()
  : lastWifiAttemptMs_(0),
    mqttClient_(wifiClient_),
    lastMqttAttemptMs_(0),
    ntpSynced_(false),
    lastNtpAttemptMs_(0) {
  instance = this;
  mqttClient_.setServer(credentials::kMqttBrokerHost, credentials::kMqttPort);
  mqttClient_.setCallback(mqttCallbackStatic);
}

void NetworkManager::begin() {
  // Attempt initial WiFi + MQTT + NTP immediately.
  ensureWiFi();
  if (isWifiConnected()) {
    ensureMqtt();
    syncNtp();
  }
}

void NetworkManager::loop() {
  mqttClient_.loop();
  ensureWiFi();
  ensureMqtt();
  processEventQueue();
  syncNtp();
}

// ── WiFi ─────────────────────────────────────────────────────────────────────

bool NetworkManager::isWifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

void NetworkManager::ensureWiFi() {
  if (isWifiConnected()) return;

  unsigned long now = millis();
  if (now - lastWifiAttemptMs_ < kWifiRetryMs) return;
  lastWifiAttemptMs_ = now;

  Serial.print("[wifi] Connecting to ");
  Serial.println(credentials::kWifiSsid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(credentials::kWifiSsid, credentials::kWifiPassword);
}

// ── MQTT ─────────────────────────────────────────────────────────────────────

void NetworkManager::ensureMqtt() {
  if (mqttClient_.connected()) {
    mqttClient_.loop();
    return;
  }

  if (!isWifiConnected()) return;

  unsigned long now = millis();
  if (now - lastMqttAttemptMs_ < kMqttRetryMs) return;
  lastMqttAttemptMs_ = now;

  Serial.print("[mqtt] Connecting to ");
  Serial.print(credentials::kMqttBrokerHost);
  Serial.print(":");
  Serial.println(credentials::kMqttPort);

  // Build LWT topic
  char lwtTopic[64];
  mqtt_topics::statePresence(lwtTopic);

  bool connected = false;

  bool hasAuth = (strlen(credentials::kMqttUsername) > 0);

  if (hasAuth) {
    connected = mqttClient_.connect(
      device_config::kDeviceId,
      credentials::kMqttUsername,
      credentials::kMqttPassword,
      lwtTopic,
      1,              // will QoS
      true,           // will retain
      kOfflinePayload // will message
    );
  } else {
    connected = mqttClient_.connect(
      device_config::kDeviceId,
      lwtTopic,
      1,
      true,
      kOfflinePayload
    );
  }

  if (connected) {
    Serial.println("[mqtt] Connected");
    subscribeToCommands();

    // Publish online presence (retained)
    publishRaw(lwtTopic, kOnlinePayload, true, 1);
  } else {
    Serial.print("[mqtt] Connection failed, rc=");
    Serial.println(mqttClient_.state());
  }
}

void NetworkManager::subscribeToCommands() {
  char topic[128];
  mqtt_topics::commandSubscribeAll(topic);
  mqttClient_.subscribe(topic);
  Serial.print("[mqtt] Subscribed to ");
  Serial.println(topic);
}

// ── MQTT Callback ────────────────────────────────────────────────────────────

void NetworkManager::mqttCallbackStatic(char* topic, byte* payload, unsigned int length) {
  if (instance) {
    instance->mqttCallback(topic, payload, length);
  }
}

void NetworkManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Null-terminate the payload
  char jsonPayload[256];
  unsigned int copyLen = (length < sizeof(jsonPayload) - 1) ? length : sizeof(jsonPayload) - 1;
  memcpy(jsonPayload, payload, copyLen);
  jsonPayload[copyLen] = '\0';

  Serial.print("[mqtt] Received on ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(jsonPayload);

  parseCommand(topic, jsonPayload);
}

// ── Command Parsing ──────────────────────────────────────────────────────────

void NetworkManager::parseCommand(const char* topic, const char* jsonPayload) {
  // Determine command type from the last segment of the topic.
  const char* lastSlash = strrchr(topic, '/');
  if (!lastSlash) {
    Serial.println("[cmd] Cannot parse topic (no slash)");
    return;
  }
  const char* commandType = lastSlash + 1;

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonPayload);
  if (error) {
    Serial.print("[cmd] JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  // Validate deviceId
  const char* deviceId = doc["deviceId"] | "";
  if (strcmp(deviceId, device_config::kDeviceId) != 0) {
    Serial.print("[cmd] Device ID mismatch: got \"");
    Serial.print(deviceId);
    Serial.print("\", expected \"");
    Serial.print(device_config::kDeviceId);
    Serial.println("\"");
    return;
  }

  // Build CommandMessage
  CommandMessage cmd;
  memset(&cmd, 0, sizeof(cmd));               // zero ALL fields first
  cmd.type = CommandMessage::Type::kSetAlarm; // then set type default

  if (strcmp(commandType, "set_alarm") == 0) {
    cmd.type = CommandMessage::Type::kSetAlarm;
    const char* alarmTime = doc["alarmTime"] | "";
    struct tm tm = {};
    // Parse RFC3339: "2030-01-01T07:00:00Z"
    if (sscanf(alarmTime, "%d-%d-%dT%d:%d:%d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
      tm.tm_year -= 1900;
      tm.tm_mon  -= 1;
      tm.tm_sec  += device_config::kTimezoneOffsetSeconds; // apply TZ offset
      cmd.alarmTimeUtc = static_cast<int64_t>(mktime(&tm));
    }
    const char* label = doc["label"] | "";
    strncpy(cmd.alarmLabel, label, sizeof(cmd.alarmLabel) - 1);

  } else if (strcmp(commandType, "display_message") == 0) {
    cmd.type = CommandMessage::Type::kDisplayMessage;
    const char* msg = doc["message"] | "";
    strncpy(cmd.message, msg, sizeof(cmd.message) - 1);
    cmd.durationSeconds = doc["durationSeconds"] | 10;

  } else if (strcmp(commandType, "set_brightness") == 0) {
    cmd.type = CommandMessage::Type::kSetBrightness;
    int level = doc["level"] | device_config::kDefaultBrightness;
    cmd.brightnessLevel = (level < 0) ? 0 : (level > 100) ? 100 : static_cast<uint8_t>(level);

  } else {
    Serial.print("[cmd] Unknown command type: ");
    Serial.println(commandType);
    return;
  }

  if (!pushCommand(cmd)) {
    Serial.println("[cmd] Command queue full, dropping message");
    // Publish a command_result event so the server knows the command wasn't applied
    EventMessage evt = {};
    evt.type = EventMessage::Type::kCommandResult;
    strncpy(evt.commandResult.commandType, commandType, sizeof(evt.commandResult.commandType) - 1);
    strncpy(evt.commandResult.status, "dropped", sizeof(evt.commandResult.status) - 1);
    strncpy(evt.commandResult.detail, "display queue full", sizeof(evt.commandResult.detail) - 1);
    xQueueSend(eventQueue, &evt, 0);
  }
}

bool NetworkManager::pushCommand(const CommandMessage& cmd) {
  BaseType_t result = xQueueSend(commandQueue, &cmd, 0);
  return result == pdTRUE;
}

// ── Event Queue Processing ───────────────────────────────────────────────────

void NetworkManager::processEventQueue() {
  EventMessage event;
  while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
    publishEventToMqtt(event);
  }
}

void NetworkManager::publishEventToMqtt(const EventMessage& event) {
  JsonDocument doc;
  char topic[128];
  bool retain = false;
  uint8_t qos = 1;

  switch (event.type) {
    case EventMessage::Type::kHeartbeat: {
      doc["uptimeSeconds"] = event.heartbeat.uptimeSeconds;
      doc["wifiRssi"]      = event.heartbeat.wifiRssi;
      doc["mqttConnected"] = mqttClient_.connected();
      doc["ntpSynced"]     = ntpSynced_;
      mqtt_topics::eventHeartbeat(topic);
      break;
    }
    case EventMessage::Type::kAlarmTriggered: {
      doc["time"]  = event.alarm.alarmTimeUtc;
      doc["label"] = event.alarm.alarmLabel;
      mqtt_topics::eventAlarmTriggered(topic);
      break;
    }
    case EventMessage::Type::kAlarmAcknowledged: {
      doc["action"]        = event.acknowledge.action;
      doc["source"]        = event.acknowledge.source;
      doc["snoozeMinutes"] = event.acknowledge.snoozeMinutes;
      mqtt_topics::eventAlarmAcknowledged(topic);
      break;
    }
    case EventMessage::Type::kCommandResult: {
      doc["type"]   = event.commandResult.commandType;
      doc["result"] = event.commandResult.status;
      if (strlen(event.commandResult.detail) > 0) {
        doc["detail"] = event.commandResult.detail;
      }
      mqtt_topics::eventCommandResult(topic);
      break;
    }
    case EventMessage::Type::kDisplayState: {
      doc["mode"]       = event.state.displayMode;
      doc["brightness"] = event.state.brightness;
      mqtt_topics::stateDisplay(topic);
      retain = true;
      break;
    }
    case EventMessage::Type::kAlarmState: {
      doc["armed"] = event.state.alarmArmed;
      mqtt_topics::stateAlarm(topic);
      retain = true;
      break;
    }
    default:
      return;
  }

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
  publishRaw(topic, jsonBuffer, retain, qos);
}

// ── NTP ──────────────────────────────────────────────────────────────────────

void NetworkManager::syncNtp() {
  if (ntpSynced_) return;
  if (!isWifiConnected()) return;

  unsigned long now = millis();
  if (now - lastNtpAttemptMs_ < kNtpRetryMs) return;
  lastNtpAttemptMs_ = now;

  Serial.print("[ntp] Syncing time from ");
  Serial.println(device_config::kNtpServer);

  configTime(
    device_config::kTimezoneOffsetSeconds,
    0,  // no DST offset
    device_config::kNtpServer
  );

  time_t nowTime = time(nullptr);
  if (nowTime > kMinNtpEpoch) {
    ntpSynced = true;
    ntpSynced_ = true;
    struct tm timeinfo;
    localtime_r(&nowTime, &timeinfo);
    Serial.print("[ntp] Time synced: ");
    Serial.println(asctime(&timeinfo));
  } else {
    Serial.println("[ntp] NTP sync failed, will retry");
  }
}

// ── Low-level publish ────────────────────────────────────────────────────────

bool NetworkManager::publishRaw(const char* topic, const char* payload, bool retained, uint8_t qos) {
  // PubSubClient publish(const char*, const char*, bool) sends at QoS 0.
  // For QoS 1 delivery, use the overload publish(const char*, const uint8_t*,
  // unsigned int, bool) or accept QoS 0 for retained/status topics.
  if (!mqttClient_.connected()) return false;
  bool ok = mqttClient_.publish(topic, payload, retained);
  if (ok) {
    Serial.print("[mqtt] Published to ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(payload);
  } else {
    Serial.print("[mqtt] Publish failed to ");
    Serial.println(topic);
  }
  return ok;
}

unsigned long NetworkManager::millis() const {
  return ::millis();
}

// ── FreeRTOS Task Entry ──────────────────────────────────────────────────────

void networkTaskEntry(void* pvParameters) {
  NetworkManager mgr;
  mgr.begin();

  for (;;) {
    mgr.loop();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
