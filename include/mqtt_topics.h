#pragma once

// Single source of truth for all MQTT topic strings (AR7, NFR18).
// No inline topic strings are permitted anywhere else in the firmware.
// Topics are constructed at runtime via snprintf using DEVICE_ID from device_config.h.

#include <cstdio>
#include "device_config.h"

namespace mqtt_topics {

// Maximum length for any constructed topic string (includes null terminator).
constexpr size_t kTopicBufferSize = 128;

// ── Command topics: firmware subscribes ────────────────────────────────────
// Pattern: clocks/commands/{deviceId}/{command}

// Wildcard subscription to receive all commands for this device (FR7).
inline void commandSubscribeAll(char* buf) {
  snprintf(buf, kTopicBufferSize, "%s/commands/%s/#",
           device_config::kMqttTopicPrefix, device_config::kDeviceId);
}

inline void commandSetAlarm(char* buf) {
  snprintf(buf, kTopicBufferSize, "%s/commands/%s/set_alarm",
           device_config::kMqttTopicPrefix, device_config::kDeviceId);
}

inline void commandDisplayMessage(char* buf) {
  snprintf(buf, kTopicBufferSize, "%s/commands/%s/display_message",
           device_config::kMqttTopicPrefix, device_config::kDeviceId);
}

inline void commandSetBrightness(char* buf) {
  snprintf(buf, kTopicBufferSize, "%s/commands/%s/set_brightness",
           device_config::kMqttTopicPrefix, device_config::kDeviceId);
}

// ── Event topics: firmware publishes ───────────────────────────────────────
// Pattern: clocks/events/{deviceId}/{event}
// Delivery rules (from docs/lcd-reference.md):
//   heartbeat:          QoS 0, retain = false
//   command_result:     QoS 1, retain = false
//   alarm_triggered:    QoS 1, retain = false
//   alarm_acknowledged: QoS 1, retain = false

inline void eventHeartbeat(char* buf) {
  snprintf(buf, kTopicBufferSize, "%s/events/%s/heartbeat",
           device_config::kMqttTopicPrefix, device_config::kDeviceId);
}

inline void eventCommandResult(char* buf) {
  snprintf(buf, kTopicBufferSize, "%s/events/%s/command_result",
           device_config::kMqttTopicPrefix, device_config::kDeviceId);
}

inline void eventAlarmTriggered(char* buf) {
  snprintf(buf, kTopicBufferSize, "%s/events/%s/alarm_triggered",
           device_config::kMqttTopicPrefix, device_config::kDeviceId);
}

inline void eventAlarmAcknowledged(char* buf) {
  snprintf(buf, kTopicBufferSize, "%s/events/%s/alarm_acknowledged",
           device_config::kMqttTopicPrefix, device_config::kDeviceId);
}

// ── State topics: firmware publishes (all retained) ────────────────────────
// Pattern: clocks/state/{deviceId}/{state}
// Delivery rules: QoS 1, retain = true
// Note: presence is a retained state topic per docs/lcd-reference.md:176,
//       not an event topic.

inline void statePresence(char* buf) {
  snprintf(buf, kTopicBufferSize, "%s/state/%s/presence",
           device_config::kMqttTopicPrefix, device_config::kDeviceId);
}

inline void stateAlarm(char* buf) {
  snprintf(buf, kTopicBufferSize, "%s/state/%s/alarm",
           device_config::kMqttTopicPrefix, device_config::kDeviceId);
}

inline void stateDisplay(char* buf) {
  snprintf(buf, kTopicBufferSize, "%s/state/%s/display",
           device_config::kMqttTopicPrefix, device_config::kDeviceId);
}

}  // namespace mqtt_topics
