# Story 1.1: Project Scaffolding & Build System

**Epic:** 1 — Connected Clock Foundation
**Story ID:** 1.1
**Status:** done
**Created:** 2026-03-23

---

## Tasks / Subtasks

- [x] Task 1: Update `platformio.ini` — add 3 missing libraries + `[env:native]`
- [x] Task 2: Update `.gitignore` — add `include/credentials.h` and `clock-server/.env`
- [x] Task 3: Create `include/credentials.h.example`
- [x] Task 4: Create `include/device_config.h`
- [x] Task 5: Create `include/mqtt_topics.h`
- [x] Task 6: Create `include/queue_types.h`
- [x] Task 7: Create stub `src/display_task.h` + `src/display_task.cpp`
- [x] Task 8: Create stub `src/network_task.h` + `src/network_task.cpp`
- [x] Task 9: Replace `src/main.cpp` with FreeRTOS task launchers only
- [x] Task 10: Verify `pio run -e hosyond_28_esp32` builds successfully

---

## Dev Agent Record

### Implementation Plan
Implemented all scaffolding files per the story spec. Key decision: `paulstoffregen/XPT2046_Touchscreen @ ^1.4` does not resolve in the PlatformIO registry (library is published as `0.0.0-alpha`); used the GitHub URL `https://github.com/PaulStoffregen/XPT2046_Touchscreen.git` instead — this resolves to v1.4.0 at build time.

The TFT_eSPI `TOUCH_CS pin not defined` warning is expected and harmless — we use `XPT2046_Touchscreen` on a separate HSPI bus; TFT_eSPI's built-in touch is intentionally not used (confirmed in architecture).

### Debug Log
- `paulstoffregen/XPT2046_Touchscreen @ ^1.4` → `UnknownPackageError`. Resolved by using GitHub URL. Final resolved version: `XPT2046_Touchscreen @ 1.4.0+sha.f956c5d`.

### Completion Notes
All 10 tasks complete. `pio run -e hosyond_28_esp32` → SUCCESS (RAM 6.4%, Flash 17.8%). `[env:native]` recognized by PlatformIO (`pio project config` confirms). `include/credentials.h` gitignored (verified with `git check-ignore`). `include/credentials.h.example` untracked (not gitignored). All new files present and correct.

---

## File List
- `platformio.ini` — modified: added XPT2046_Touchscreen (GitHub URL), PubSubClient, ArduinoJson libs; added `[env:native]`
- `.gitignore` — modified: added `include/credentials.h` and `clock-server/.env`
- `include/credentials.h.example` — created: credential template (tracked)
- `include/device_config.h` — created: DEVICE_ID, NTP_SERVER, topic prefix, timezone offset
- `include/mqtt_topics.h` — created: all 12 MQTT topic constructor functions in `mqtt_topics` namespace
- `include/queue_types.h` — created: `CommandMessage` + `EventMessage` structs, queue extern handles
- `src/display_task.h` — created: stub header
- `src/display_task.cpp` — created: stub implementation
- `src/network_task.h` — created: stub header
- `src/network_task.cpp` — created: stub implementation
- `src/main.cpp` — replaced: now only creates queues + `xTaskCreatePinnedToCore()` for DisplayTask and NetworkTask
- `_bmad-output/implementation-artifacts/sprint-status.yaml` — modified: epic-1 in-progress, story in-progress

---

## Change Log
- 2026-03-23: Story 1.1 implemented — project scaffolding, build system, and shared contracts established. All 4 libraries resolved. FreeRTOS dual-task architecture in place with stub task implementations ready for Story 1.2+.

---

## User Story

As a developer,
I want all project configuration, shared contract types, and library dependencies in place,
So that the firmware can be built and flashed to the device with documented steps.

---

## Acceptance Criteria

**AC1 — Build succeeds:**
Given the repo is freshly cloned,
When `cp include/credentials.h.example include/credentials.h` is run and credentials are filled in,
Then `pio run -e hosyond_28_esp32` completes without errors.

**AC2 — Library dependencies:**
Given `platformio.ini` is reviewed, when `lib_deps` is inspected,
Then it includes `bodmer/TFT_eSPI@^2.5.43`, `paulstoffregen/XPT2046_Touchscreen@^1.4`, `knolleary/PubSubClient@^2.8`, and `bblanchon/ArduinoJson@^7.0`.

**AC3 — Native test environment:**
Given `platformio.ini` is reviewed, when the `[env:native]` section is inspected,
Then it exists with `platform = native` and `bblanchon/ArduinoJson@^7.0` as a lib_dep.

**AC4 — Gitignore:**
Given `include/credentials.h` exists, when `.gitignore` is reviewed,
Then `include/credentials.h` and `clock-server/.env` are gitignored, and `include/credentials.h.example` is tracked.

**AC5 — MQTT topic constants:**
Given `include/mqtt_topics.h` is reviewed, when all topic constants are inspected,
Then it defines string constants for all command topics (set_alarm, display_message, set_brightness), event topics (presence, heartbeat, command_result, alarm_triggered, alarm_acknowledged), and state topics (alarm, display) using `DEVICE_ID` from `device_config.h` — no inline topic strings anywhere else.

**AC6 — main.cpp has no logic:**
Given `src/main.cpp` is reviewed, when its contents are inspected,
Then it contains only `xTaskCreatePinnedToCore()` calls for `DisplayTask` (Core 1, high priority) and `NetworkTask` (Core 0, normal priority), and no other logic.

**AC7 — Inter-task queue types:**
Given shared inter-task message types are defined, when the `CommandMessage` and `EventMessage` struct definitions are reviewed,
Then both enums and all field types are defined, and `commandQueue` (capacity 8) and `eventQueue` (capacity 16) handle types are declared in a shared header.

---

## Critical Developer Context

### What Already Exists — Do NOT Recreate

- **`include/board_config.h`** — Complete GPIO pin map (all `board::kPascalCase` constants). Never modify or recreate this file. All GPIO constants come from here.
- **`platformio.ini` `[env:hosyond_28_esp32]`** — The main firmware env with TFT_eSPI + all build_flags. ADD libraries to `lib_deps`; do not remove existing entries.
- **`src/main.cpp`** — Existing bring-up code (TFT test screen, RGB LED blink). This file MUST be **completely replaced** with FreeRTOS task launchers only.
- **`.gitignore`** — Currently only has `.pio/` and `.vscode/`. Must ADD entries; do not remove existing ones.

### Code Conventions — Follow Exactly

These are established by `board_config.h` and the architecture. Deviations break consistency:

- **Constants:** `kPascalCase` in namespaces (e.g., `kLedRed`, `kAlarmRinging`) — **never** `ALL_CAPS` macros
- **File names:** `snake_case.h` / `snake_case.cpp` (e.g., `queue_types.h`, `display_task.h`)
- **Classes:** `PascalCase` (e.g., `DisplayTask`, `CommandMessage`)
- **Methods:** `camelCase` (e.g., `handleCommand()`)
- **Namespaces:** `snake_case` (e.g., `namespace board`, `namespace mqtt`)
- **Enums:** `PascalCase` type, `kPascalCase` values (e.g., `enum class DisplayMode { kClock, kMessage }`)
- **FreeRTOS queues:** `camelCase` + `Queue` suffix (e.g., `commandQueue`, `eventQueue`)

---

## Implementation Guide

### Step 1: Update `platformio.ini`

**Add to `[env:hosyond_28_esp32]` lib_deps** (TFT_eSPI is already there):
```ini
lib_deps =
  bodmer/TFT_eSPI @ ^2.5.43
  paulstoffregen/XPT2046_Touchscreen @ ^1.4
  knolleary/PubSubClient @ ^2.8
  bblanchon/ArduinoJson @ ^7.0
```

**Add new `[env:native]` section** at the end of `platformio.ini`:
```ini
[env:native]
platform = native
lib_deps =
  bblanchon/ArduinoJson @ ^7.0
```

Notes:
- `[env:native]` is for host-based unit tests (`pio test -e native`) — no Arduino headers, no ESP32 headers
- Only ArduinoJson goes here; PubSubClient and TFT_eSPI are hardware-specific and cannot build on native
- PlatformIO test runner uses Unity by default; no extra `test_framework` setting needed

### Step 2: Update `.gitignore`

Add these entries (do NOT remove existing `.pio/` and `.vscode/`):
```
include/credentials.h
clock-server/.env
```

### Step 3: Create `include/credentials.h.example`

Template file (tracked in git). Developer copies to `include/credentials.h` and fills in values:
```cpp
#pragma once

// Copy this file to credentials.h and fill in your values.
// credentials.h is gitignored — never commit it.
namespace credentials {

constexpr char kWifiSsid[]     = "YOUR_WIFI_SSID";
constexpr char kWifiPassword[] = "YOUR_WIFI_PASSWORD";

constexpr char kMqttBrokerUrl[] = "mqtts://YOUR_BROKER_HOST";
constexpr uint16_t kMqttPort    = 8883;
constexpr char kMqttUsername[]  = "YOUR_MQTT_USERNAME";
constexpr char kMqttPassword[]  = "YOUR_MQTT_PASSWORD";

// Optional: CA certificate for TLS broker verification.
// Set to nullptr to skip server cert validation (insecure, local dev only).
constexpr const char* kMqttCaCert = nullptr;

}  // namespace credentials
```

### Step 4: Create `include/device_config.h`

Tracked configuration (non-secret). This file is the single source of truth for device identity:
```cpp
#pragma once

namespace device_config {

constexpr char kDeviceId[]        = "clock-1";          // Change per device instance
constexpr char kNtpServer[]       = "pool.ntp.org";     // NFR19: configurable, not hardcoded
constexpr char kMqttTopicPrefix[] = "clocks";
constexpr int8_t kTimezoneOffsetHours = 0;              // UTC offset; adjust for local timezone

}  // namespace device_config
```

**Important:** `DEVICE_ID` used in `mqtt_topics.h` refers to `device_config::kDeviceId`. This is how FR31/FR33 (config-only multi-device) works — change only this file per device.

### Step 5: Create `include/mqtt_topics.h`

**CRITICAL ARCHITECTURE RULE:** This is the sole source of truth for all MQTT topic strings. No inline topic strings anywhere else in the firmware. Topics are constructed at runtime via `snprintf` using `device_config::kDeviceId`.

```cpp
#pragma once

#include <cstdio>
#include "device_config.h"

namespace mqtt_topics {

// Buffer size for constructed topic strings
constexpr size_t kTopicBufferSize = 128;

// ── Command topics (firmware subscribes) ───────────────────────────────────
// Pattern: clocks/commands/{deviceId}/{command}
inline void commandSetAlarm(char* buf)        { snprintf(buf, kTopicBufferSize, "clocks/commands/%s/set_alarm",        device_config::kDeviceId); }
inline void commandDisplayMessage(char* buf)  { snprintf(buf, kTopicBufferSize, "clocks/commands/%s/display_message",  device_config::kDeviceId); }
inline void commandSetBrightness(char* buf)   { snprintf(buf, kTopicBufferSize, "clocks/commands/%s/set_brightness",   device_config::kDeviceId); }
// Wildcard subscription to catch all commands for this device
inline void commandSubscribeAll(char* buf)    { snprintf(buf, kTopicBufferSize, "clocks/commands/%s/#",                device_config::kDeviceId); }

// ── Event topics (firmware publishes) ──────────────────────────────────────
// Pattern: clocks/events/{deviceId}/{event}
inline void eventPresence(char* buf)          { snprintf(buf, kTopicBufferSize, "clocks/events/%s/presence",          device_config::kDeviceId); }
inline void eventHeartbeat(char* buf)         { snprintf(buf, kTopicBufferSize, "clocks/events/%s/heartbeat",         device_config::kDeviceId); }
inline void eventCommandResult(char* buf)     { snprintf(buf, kTopicBufferSize, "clocks/events/%s/command_result",    device_config::kDeviceId); }
inline void eventAlarmTriggered(char* buf)    { snprintf(buf, kTopicBufferSize, "clocks/events/%s/alarm_triggered",   device_config::kDeviceId); }
inline void eventAlarmAcknowledged(char* buf) { snprintf(buf, kTopicBufferSize, "clocks/events/%s/alarm_acknowledged",device_config::kDeviceId); }

// ── State topics (firmware publishes, retained) ────────────────────────────
// Pattern: clocks/state/{deviceId}/{state}
inline void stateAlarm(char* buf)             { snprintf(buf, kTopicBufferSize, "clocks/state/%s/alarm",   device_config::kDeviceId); }
inline void stateDisplay(char* buf)           { snprintf(buf, kTopicBufferSize, "clocks/state/%s/display", device_config::kDeviceId); }

}  // namespace mqtt_topics
```

**Delivery rules per `docs/lcd-reference.md`** (developer must follow when publishing):
| Topic | QoS | Retain |
|---|---|---|
| `clocks/commands/...` | 1 | false |
| `.../heartbeat` | 0 | false |
| `.../alarm_triggered` | 1 | false |
| `.../alarm_acknowledged` | 1 | false |
| `.../command_result` | 0 or 1 | false |
| `clocks/state/...` | 1 | true |

### Step 6: Create `include/queue_types.h`

This header defines the inter-task message contracts. Must be included by `main.cpp`, `display_task.h`, and `network_task.h`. Placed in `include/` so both `src/` files and future `test/` files can access it.

**Critical AR1:** These types must be defined before either task is implemented — they are the inter-task API.

```cpp
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// ── Command messages: NetworkTask → DisplayTask ────────────────────────────
struct CommandMessage {
  enum class Type { kSetAlarm, kDisplayMessage, kSetBrightness };
  Type type;

  // Set alarm fields
  char alarmLabel[64];       // label string, null-terminated
  time_t alarmTimeUtc;       // Unix timestamp (UTC)

  // Display message fields
  char message[160];         // message text, null-terminated
  uint32_t durationSeconds;  // how long to show the message

  // Set brightness fields
  uint8_t brightnessLevel;   // 0–100
};

// ── Event messages: DisplayTask → NetworkTask ──────────────────────────────
struct EventMessage {
  enum class Type { kAlarmTriggered, kAlarmAcknowledged, kCommandResult, kDisplayState, kAlarmState };
  Type type;

  // For kAlarmTriggered / kAlarmAcknowledged
  time_t alarmTimeUtc;
  char alarmLabel[64];
  char action[16];    // e.g. "dismiss"
  char source[16];    // e.g. "touch"

  // For kCommandResult
  char commandType[32];  // e.g. "set_alarm"
  char status[16];       // "received"|"applied"|"rejected"|"failed"
  char detail[64];       // optional detail string, e.g. "device_id_mismatch"

  // For kDisplayState
  char displayMode[32];  // e.g. "clock", "message", "alarm_ringing"
  uint8_t brightness;

  // For kAlarmState
  bool alarmArmed;
};

// ── Queue handles (defined in main.cpp, extern-declared here) ──────────────
extern QueueHandle_t commandQueue;  // NetworkTask → DisplayTask, capacity 8
extern QueueHandle_t eventQueue;    // DisplayTask → NetworkTask, capacity 16
```

### Step 7: Create stub task headers and sources

These stubs exist solely to make `main.cpp` compile. They will be fully implemented in Stories 1.2 and 1.3.

**`src/display_task.h`:**
```cpp
#pragma once

// DisplayTask: runs on Core 1 (app core), high priority.
// Owns: TFT rendering, display state machine, touch polling, RGB LED, audio, alarm evaluation.
// Full implementation: Story 1.2+
void displayTaskEntry(void* pvParameters);
```

**`src/display_task.cpp`:**
```cpp
#include "display_task.h"
#include <Arduino.h>

void displayTaskEntry(void* pvParameters) {
  // TODO Story 1.2: implement display task
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
```

**`src/network_task.h`:**
```cpp
#pragma once

// NetworkTask: runs on Core 0 (proto core), normal priority.
// Owns: Wi-Fi management, MQTT connect/reconnect, NTP sync, inbound message receipt.
// Full implementation: Story 1.3+
void networkTaskEntry(void* pvParameters);
```

**`src/network_task.cpp`:**
```cpp
#include "network_task.h"
#include <Arduino.h>

void networkTaskEntry(void* pvParameters) {
  // TODO Story 1.3: implement network task
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
```

### Step 8: Replace `src/main.cpp`

**COMPLETELY REPLACE** the existing bring-up code. The new `main.cpp` has no logic — only queue creation and `xTaskCreatePinnedToCore()` calls. This satisfies AR5.

```cpp
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "queue_types.h"
#include "display_task.h"
#include "network_task.h"

// Queue handles — defined here, extern-declared in queue_types.h
QueueHandle_t commandQueue;
QueueHandle_t eventQueue;

void setup() {
  commandQueue = xQueueCreate(8, sizeof(CommandMessage));
  eventQueue   = xQueueCreate(16, sizeof(EventMessage));

  // DisplayTask on Core 1 (app core), high priority (5)
  xTaskCreatePinnedToCore(
    displayTaskEntry,
    "DisplayTask",
    8192,    // stack size in bytes
    nullptr,
    5,       // priority (high)
    nullptr,
    1        // core 1
  );

  // NetworkTask on Core 0 (proto core), normal priority (1)
  xTaskCreatePinnedToCore(
    networkTaskEntry,
    "NetworkTask",
    8192,    // stack size in bytes
    nullptr,
    1,       // priority (normal)
    nullptr,
    0        // core 0
  );
}

void loop() {
  // FreeRTOS tasks own all work. loop() is unused.
  vTaskDelete(nullptr);
}
```

---

## Files To Create / Modify

| Action | File | Notes |
|---|---|---|
| MODIFY | `platformio.ini` | Add 3 libs to hosyond env; add `[env:native]` |
| MODIFY | `.gitignore` | Add `include/credentials.h` and `clock-server/.env` |
| CREATE | `include/credentials.h.example` | Tracked template |
| CREATE | `include/device_config.h` | Device identity constants |
| CREATE | `include/mqtt_topics.h` | All MQTT topic constants |
| CREATE | `include/queue_types.h` | FreeRTOS queue message structs + extern handles |
| REPLACE | `src/main.cpp` | Remove bring-up logic; only task launches |
| CREATE | `src/display_task.h` | Stub header |
| CREATE | `src/display_task.cpp` | Stub implementation |
| CREATE | `src/network_task.h` | Stub header |
| CREATE | `src/network_task.cpp` | Stub implementation |

**Do NOT create:** `include/credentials.h` (gitignored; only `.example` is committed)

---

## Architecture Guardrails

**MUST follow:**
- `main.cpp` contains ZERO application logic — only queue creation + `xTaskCreatePinnedToCore()` calls
- No inline MQTT topic strings anywhere — all topics via `mqtt_topics.h` functions
- `board_config.h` is authoritative for GPIO — never define pin numbers elsewhere
- Constants use `kPascalCase` in namespaces — never `#define` for application constants
- `include/credentials.h` must NEVER be committed (gitignored)
- `credentials.h.example` must be committed and kept up to date with all required fields

**Cross-story contracts established in this story (future stories depend on these):**
- `CommandMessage` field names and types in `queue_types.h` — Stories 1.3, 2.1, 2.2, 2.3, 3.1 all use this
- `EventMessage` field names and types — Stories 1.5, 2.1–2.3, 3.x all use this
- `mqtt_topics.h` namespace + function signatures — Every story that publishes/subscribes uses this
- `device_config::kDeviceId` constant name — Referenced throughout firmware

**Do NOT implement in this story:**
- Any actual Wi-Fi, MQTT, NTP, or display logic (that's Stories 1.2–1.5)
- Unit tests (that's Story 3.4)
- `clock-server` backend code (that's Epic 4)

---

## Verification

After implementation, verify:
1. `pio run -e hosyond_28_esp32` succeeds (after creating `credentials.h` from `.example`)
2. `git status` confirms `include/credentials.h` is untracked (gitignored)
3. `include/credentials.h.example` is tracked
4. `platformio.ini` has all 4 libs in hosyond env AND `[env:native]` section
5. `src/main.cpp` contains no logic beyond queue creation and `xTaskCreatePinnedToCore()` calls
6. `include/mqtt_topics.h` has functions for all 12 topics; no inline strings in any other file

---

## Dev Notes

_To be filled in by the developer during/after implementation._
