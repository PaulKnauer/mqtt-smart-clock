#pragma once

// Device-level configuration. This file is tracked in version control.
// Change kDeviceId per device instance — no other code changes required (FR33).

namespace device_config {

// Unique device identity. Used to scope all MQTT topic subscriptions and publications (FR31).
// Must match clock-server device registration. Allowed chars: a-z, A-Z, 0-9, _, -
constexpr char kDeviceId[] = "clock-1";

// NTP server address. Configurable — never hardcoded in source (NFR19).
constexpr char kNtpServer[] = "pool.ntp.org";

// MQTT topic root prefix.
constexpr char kMqttTopicPrefix[] = "clocks";

// UTC offset in hours for local time display.
// Adjust for your timezone (e.g., -5 for EST, 1 for CET).
constexpr int8_t kTimezoneOffsetHours = 0;

// UTC offset in seconds (derived — do not change directly).
constexpr long kTimezoneOffsetSeconds = static_cast<long>(kTimezoneOffsetHours) * 3600L;

}  // namespace device_config
