#pragma once

// Inter-task message contract (AR1).
// Defines CommandMessage (NetworkTask → DisplayTask) and EventMessage (DisplayTask → NetworkTask).
// These types MUST be defined before either task is implemented — they are the inter-task API.
// All cross-task data must transit these queues; no direct cross-task function calls (AR6).
//
// This header is intentionally FREE of FreeRTOS and Arduino dependencies so that
// host-based unit tests (pio test -e native) can include it without a firmware toolchain.
// Queue handle declarations live in queue_handles.h (firmware-only).

#include <ctime>
#include <cstdint>

// ── Command messages: NetworkTask → DisplayTask ────────────────────────────
// commandQueue capacity: 8 messages
struct CommandMessage {
  enum class Type { kSetAlarm, kDisplayMessage, kSetBrightness };
  Type type;

  // Fields for kSetAlarm
  char alarmLabel[64];    // alarm label string, null-terminated
  time_t alarmTimeUtc;    // scheduled alarm time as Unix timestamp (UTC)

  // Fields for kDisplayMessage
  char message[160];         // message text to display, null-terminated
  uint32_t durationSeconds;  // how long to show the message before returning to clock

  // Fields for kSetBrightness
  uint8_t brightnessLevel;  // backlight level 0–100
};

// ── Event messages: DisplayTask → NetworkTask ──────────────────────────────
// eventQueue capacity: 16 messages
struct EventMessage {
  enum class Type {
    kAlarmTriggered,
    kAlarmAcknowledged,
    kCommandResult,
    kDisplayState,
    kAlarmState
  };
  Type type;

  // Fields for kAlarmTriggered / kAlarmAcknowledged
  time_t alarmTimeUtc;  // scheduled alarm time
  char alarmLabel[64];  // alarm label
  char action[16];      // e.g. "dismiss" or "snooze"
  char source[16];      // e.g. "touch"

  // Fields for kCommandResult
  char commandType[32];  // e.g. "set_alarm", "display_message", "set_brightness"
  char status[16];       // "received" | "applied" | "rejected" | "failed"
  char detail[64];       // optional detail, e.g. "device_id_mismatch", "json_parse_error"

  // Fields for kDisplayState
  char displayMode[32];  // e.g. "clock", "message", "alarm_ringing", "error"
  uint8_t brightness;    // current backlight level 0–100

  // Fields for kAlarmState
  bool alarmArmed;       // true if alarm is scheduled and not yet triggered
};


// Queue handle declarations are in queue_handles.h (firmware-only, includes FreeRTOS).
// Firmware task files that call xQueueSend/xQueueReceive should include queue_handles.h.
// Native test files include only this header.
