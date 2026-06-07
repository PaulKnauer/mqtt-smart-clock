---
stepsCompleted: [1, 2, 3, 4, 5, 6, 7, 8]
lastStep: 8
inputDocuments:
  - project-docs/planning-artifacts/prds/prd-mqtt-smart-clock-2026-06-06/prd.md
  - project-docs/planning-artifacts/briefs/brief-mqtt-smart-clock-2026-06-06/brief.md
workflowType: 'architecture'
project_name: 'mqtt-smart-clock'
user_name: 'Paul'
date: '2026-06-06'
status: 'complete'
completedAt: '2026-06-06'
---

# Architecture Decision Document

## Project Context Analysis

### Requirements Overview

**Functional Requirements:**

The firmware implements 12 functional requirements across six areas:
- Network Connectivity (FR1-3): WiFi, MQTT, NTP with auto-reconnect.
- Display (FR4-5): Clock face, multi-priority display modes (alarm > message > clock > error).
- Commands (FR6-8): set_alarm, display_message, set_brightness with validation and result reporting.
- Alarm Ringing (FR9-10): Buzzer + LED + touch dismiss/snooze.
- Device Events (FR11-12): Heartbeat, presence (LWT).

**Non-Functional Requirements:**

- Dual-core FreeRTOS with strict task isolation.
- Interface-based abstraction for testability.
- Compile-time configuration (no runtime config portal).
- PlatformIO build system with multi-environment support.
- Native unit tests with Unity test framework.

**Scale & Complexity:**

- Single-device firmware (not multi-device orchestration).
- Real-time constraints: alarm timing, display refresh, touch response.
- Resource-constrained: ESP32 with ~320KB RAM, 4MB flash.

### Technical Constraints & Dependencies

**Hardware (Hosyond 2.8" ESP32-32E):**
- MCU: ESP32 (dual-core Xtensa LX6, 240MHz)
- Display: ILI9341 240×320 TFT LCD (SPI)
- Touch: XPT2046 resistive touch (separate SPI bus)
- Audio: DAC on GPIO26, enable on GPIO4
- RGB LED: GPIO22(R)/16(G)/17(B), active low
- MicroSD: GPIO5/23/18/19 (separate SPI, unused in v1)
- Backlight: GPIO21 (active high)

**Software stack:**
- **Platform:** `espressif32` via PlatformIO
- **Board:** `esp32dev`
- **Framework:** Arduino (ESP32 Arduino Core)
- **RTOS:** FreeRTOS (built into ESP32 Arduino Core)
- **Display:** `TFT_eSPI ^2.5.43` (Bodmer)
- **Touch:** `XPT2046_Touchscreen` (PaulStoffregen)
- **MQTT:** `PubSubClient ^2.8` (knolleary)
- **JSON:** `ArduinoJson ^7.0` (bblanchon)
- **Testing:** `Unity` test framework (PlatformIO built-in)

**MQTT contract (from clock-server/mqtt-mcp-server):**
- Topic pattern: `{prefix}/{category}/{deviceId}/{subtopic}`
- Commands: `clocks/commands/{deviceId}/{commandType}`
- State: `clocks/state/{deviceId}/{thing}` (retained)
- Events: `clocks/events/{deviceId}/{eventType}` (non-retained)
- Default prefix: `clocks`
- Default device ID: `clock-1`

## Core Architectural Decisions

### Decision: Dual-Core FreeRTOS with Queue Isolation

**Core 0 (Protocol Core) — NetworkTask:**
- Owns: WiFi, MQTT, NTP.
- Never accesses the display or touch controller.
- Reads MQTT messages, parses JSON, pushes `CommandMessage` to commandQueue.
- Receives `EventMessage` from eventQueue, serializes, publishes to MQTT.

**Core 1 (Application Core) — DisplayTask:**
- Owns: ILI9341, XPT2046, audio buzzer, RGB LED, RTC, alarm state.
- Never calls WiFi, MQTT, or blocking network functions.
- Reads `CommandMessage` from commandQueue, applies to display state machine.
- Pushes `EventMessage` to eventQueue for MQTT publication.

**Queues:**
- Command Queue (8 slots): NetworkTask → DisplayTask. Carries parsed command data.
- Event Queue (16 slots): DisplayTask → NetworkTask. Carries events for publication.

### Decision: Interface-Based Abstraction

All hardware dependencies are hidden behind interfaces for testability:

- `IDisplay` — ILI9341 rendering (clock face, message overlay, alarm screen)
- `ITouch` — XPT2046 touch input (coordinate mapping, gesture detection)
- `IAlarmStore` — NVS-backed alarm persistence
- `IBacklight` — PWM backlight control with level persistence

### Decision: Display State Machine

Priority-ordered modes:

```
┌──────────────────────────────────────────────┐
│               IDLE (clock face)               │
│  ← default state, time display                │
├──────────────────────────────────────────────┤
│           MESSAGE OVERLAY                     │
│  ← triggered by display_message command        │
│  ← auto-dismiss after durationSeconds          │
│  ← returns to IDLE                             │
├──────────────────────────────────────────────┤
│           ALARM RINGING (highest)             │
│  ← triggered by scheduled alarm                │
│  ← shows dismiss/snooze buttons               │
│  ← on dismiss: returns to IDLE                 │
│  ← on snooze: re-arms, returns to IDLE         │
├──────────────────────────────────────────────┤
│           ERROR (optional)                     │
│  ← shown when NTP/MQTT lost for >30s           │
│  ← auto-clears on reconnect                    │
└──────────────────────────────────────────────┘
```

### Decision: Compile-Time Configuration

Same pattern as MqttDht22:
- `include/credentials.h.example` — template file, documented placeholders
- `credentials.h` — gitignored, contains actual WiFi SSID/password, MQTT credentials
- PlatformIO build_flags can override defaults (for multi-device builds)
- Device ID, timezone, MQTT prefix in `device_config.h` with `#ifndef` guards

## Implementation Patterns & Consistency Rules

### Project Structure

```
mqtt-smart-clock/
├── platformio.ini
├── .gitignore
├── README.md
├── include/
│   ├── credentials.h.example
│   ├── board_config.h        # Pin mappings (immutable)
│   ├── device_config.h       # Device identity, timezone, MQTT prefix
│   ├── mqtt_topics.h         # Topic string builders
│   ├── queue_types.h         # CommandMessage, EventMessage structs
│   ├── queue_handles.h       # Queue handle externs
│   ├── display/
│   │   ├── IDisplay.h        # Display interface
│   │   ├── ClockFace.h       # Clock face renderer
│   │   └── DisplayManager.h  # Display state machine
│   ├── touch/
│   │   └── ITouch.h          # Touch interface
│   ├── network/
│   │   ├── INetworkClient.h  # WiFi + MQTT interface
│   │   └── NetworkManager.h  # Network task logic
│   └── alarm/
│       ├── IAlarmStore.h     # Alarm persistence interface
│       └── AlarmManager.h    # Alarm evaluation + triggering
├── src/
│   ├── main.cpp              # Entry: create queues, pin tasks
│   ├── network_task.cpp      # Core 0 loop
│   ├── display_task.cpp      # Core 1 loop
│   ├── display/
│   │   ├── ClockFace.cpp
│   │   └── DisplayManager.cpp
│   ├── network/
│   │   └── NetworkManager.cpp
│   └── alarm/
│       └── AlarmManager.cpp
├── test/
│   ├── native/               # Native (host) unit tests
│   │   ├── platformio.ini    # Test env config
│   │   └── test_*.cpp
│   └── hardware/             # Hardware-specific integration tests
│       └── platformio.ini
├── docs/
│   └── lcd-reference.md      # MQTT contract spec
└── project-docs/             # Planning artifacts
```

### Naming Patterns

- Types: `PascalCase` (e.g., `DisplayManager`, `CommandMessage`)
- Functions: `camelCase` (e.g., `renderClockFace()`, `publishHeartbeat()`)
- Files: `snake_case` (e.g., `network_task.cpp`, `mqtt_topics.h`)
- Macros: `UPPER_SNAKE_CASE` (e.g., `MQTT_TOPIC_PREFIX`, `WLAN_SSID`)
- Interfaces: `I` prefix (e.g., `IDisplay`, `ITouch`, `IAlarmStore`)

### Task Communication Contract

**CommandMessage types (NetworkTask → DisplayTask):**

```cpp
enum class CommandType { kSetAlarm, kDisplayMessage, kSetBrightness };

struct CommandMessage {
    CommandType type;
    String deviceId;      // validated before queuing
    String alarmTimeIso;  // RFC3339 string (set_alarm only)
    String alarmLabel;    // optional label (set_alarm only)
    String message;       // text (display_message only)
    int durationSeconds;  // 1-3600 (display_message only)
    int brightnessLevel;  // 0-100 (set_brightness only)
    uint32_t timestampMs; // when the command was received
};
```

**EventMessage types (DisplayTask → NetworkTask):**

```cpp
enum class EventType { kAlarmTriggered, kAlarmAcknowledged, kCommandResult, kDisplayState, kAlarmState };

struct EventMessage {
    EventType type;
    String data;              // JSON payload to publish
    String topic;             // override topic (empty = use default)
    bool retain;              // retain flag
    uint8_t qos;              // 0 or 1
};
```

### Persistence

- **Brightness:** stored in NVS (`Preferences` library), loaded on boot.
- **Alarm:** stored in NVS, loaded on boot. Single alarm supported in v1.
- **Time:** RTC maintained by ESP32's internal RTC, synced via NTP on boot.

### Error Handling

- Network: retry loops with 5-second intervals, counter-based backoff.
- MQTT message: JSON parse errors → publish `command_result` with `rejected` + reason.
- Display: display_task reports errors via eventQueue (can't render, out of memory).
- All tasks: watchdog timer prevents lockups.

### MQTT Topic Builders

All topic strings are built by `mqtt_topics.h` helper functions:

```cpp
String commandTopic(const char* deviceId, const char* commandType);
String stateTopic(const char* deviceId, const char* thing);
String eventTopic(const char* deviceId, const char* eventType);
```

This is the single source of truth — no ad-hoc topic string construction elsewhere.

### Enforcement Guidelines

- NetworkTask **never** imports display or touch headers.
- DisplayTask **never** imports MQTT or WiFi headers.
- All queue message types go in `queue_types.h` — no new structs scattered across files.
- All MQTT topics go through `mqtt_topics.h` — no string literals in logic code.
- All delays in tasks use `vTaskDelay` — never `delay()` in task context.
- `Serial` logging uses `#ifdef DEBUG` guards.
