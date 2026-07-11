#pragma once

#include <cstdio>
#include <cstring>
#include <ctime>

namespace mqtt_contract {

constexpr size_t kTimestampSize = 21;

inline bool formatUtcTimestamp(time_t epoch, char* buffer, size_t bufferSize) {
  if (bufferSize < kTimestampSize) return false;

  struct tm utc = {};
  if (gmtime_r(&epoch, &utc) == nullptr) return false;
  return strftime(buffer, bufferSize, "%Y-%m-%dT%H:%M:%SZ", &utc) == kTimestampSize - 1;
}

inline bool buildPresencePayload(
    char* buffer,
    size_t bufferSize,
    const char* deviceId,
    const char* state,
    time_t observedAt) {
  char timestamp[kTimestampSize];
  if (!formatUtcTimestamp(observedAt, timestamp, sizeof(timestamp))) return false;

  int written = snprintf(
      buffer,
      bufferSize,
      "{\"deviceId\":\"%s\",\"state\":\"%s\",\"at\":\"%s\"}",
      deviceId,
      state,
      timestamp);
  return written >= 0 && static_cast<size_t>(written) < bufferSize;
}

inline const char* canonicalCommandResultStatus(const char* status) {
  if (strcmp(status, "received") == 0 ||
      strcmp(status, "applied") == 0 ||
      strcmp(status, "rejected") == 0 ||
      strcmp(status, "failed") == 0) {
    return status;
  }
  return "failed";
}

}  // namespace mqtt_contract
