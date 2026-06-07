#pragma once

#include <stdint.h>
#include <stddef.h>  // for size_t

// ── Cross-task message types ─────────────────────────────────────────────────
// Shared between NetworkTask (Core 0) and DisplayTask (Core 1).
// All times stored as int64_t (Unix epoch seconds) for platform-independent size.

// Commands sent *to* the display (from NetworkTask → DisplayTask).
struct CommandMessage {
  enum class Type : uint8_t {
    kSetAlarm,
    kDisplayMessage,
    kSetBrightness,
  } type;

  // Alarm command fields
  int64_t alarmTimeUtc;    // Unix epoch seconds (int64_t for platform-independence)
  char    alarmLabel[64];

  // Display command fields
  char   message[128];
  int32_t durationSeconds;

  // Brightness command fields
  uint8_t brightnessLevel;
};
static_assert(sizeof(CommandMessage) <= 256,
              "CommandMessage exceeds 256 bytes — increase queue item size or reduce fields");

// Events published *from* the display (DisplayTask → NetworkTask → MQTT).
struct EventMessage {
  enum class Type : uint8_t {
    kHeartbeat,          // 30-second periodic telemetry
    kCommandResult,      // Result of processing a command
    kAlarmTriggered,     // Alarm started ringing
    kAlarmAcknowledged,  // Alarm dismissed/snoozed/timeout
    kDisplayState,       // Current display mode + brightness (retained)
    kAlarmState,         // Alarm armed/disarmed (retained)
  } type;

  union {
    // kHeartbeat
    struct {
      uint32_t uptimeSeconds;
      int16_t  wifiRssi;
      bool     mqttConnected;
      bool     ntpSynced;
    } heartbeat;

    // kCommandResult
    struct {
      char commandType[32];
      char status[16];
      char detail[64];
    } commandResult;

    // kAlarmTriggered / kAlarmState
    struct {
      int64_t alarmTimeUtc;
      char    alarmLabel[64];
    } alarm;

    // kAlarmAcknowledged
    struct {
      char action[16];  // "dismiss", "snooze", "timeout"
      char source[16];  // "timer", "touch"
      int16_t snoozeMinutes;
    } acknowledge;

    // kDisplayState
    struct {
      uint8_t brightness;
      char    displayMode[16];
      bool    alarmArmed;  // Also used by kAlarmState
    } state;
  };
};
static_assert(sizeof(EventMessage) <= 256,
              "EventMessage exceeds 256 bytes — increase queue item size or reduce fields");
