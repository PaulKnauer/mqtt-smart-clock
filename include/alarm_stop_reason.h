#pragma once

#include <stdint.h>

enum class AlarmStopReason : uint8_t {
  kDismiss,
  kSnooze,
  kTimeout,
};

inline const char* alarmStopAction(AlarmStopReason reason) {
  switch (reason) {
    case AlarmStopReason::kDismiss:
      return "dismiss";
    case AlarmStopReason::kSnooze:
      return "snooze";
    case AlarmStopReason::kTimeout:
      // The MQTT contract permits dismiss/snooze; source distinguishes auto-timeout.
      return "dismiss";
  }
  return "dismiss";
}

inline const char* alarmStopSource(AlarmStopReason reason) {
  switch (reason) {
    case AlarmStopReason::kDismiss:
    case AlarmStopReason::kSnooze:
      return "button";
    case AlarmStopReason::kTimeout:
      return "timer";
  }
  return "timer";
}

inline int16_t alarmStopSnoozeMinutes(AlarmStopReason reason) {
  return reason == AlarmStopReason::kSnooze ? 5 : 0;
}

inline bool alarmStopClearsAlarm(AlarmStopReason reason) {
  return reason != AlarmStopReason::kSnooze;
}
