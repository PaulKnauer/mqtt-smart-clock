---
stepsCompleted: [1, 2, 3, 4, 5, 6, 7, 8]
lastStep: 8
status: 'complete'
completedAt: '2026-03-22'
inputDocuments: ['_bmad-output/planning-artifacts/prd.md', 'docs/lcd-reference.md']
workflowType: 'architecture'
project_name: 'mqtt-smart-clock'
user_name: 'Paul'
date: '2026-03-22'
---

# Architecture Decision Document

_This document builds collaboratively through step-by-step discovery. Sections are appended as we work through each architectural decision together._

## Project Context Analysis

### Requirements Overview

**Functional Requirements:** 40 FRs across 7 capability areas — Time & Display (FR1–6), MQTT Command Handling (FR7–14), Alarm (FR15–21), Connectivity & Resilience (FR22–27), Observability & Diagnostics (FR28–30), Configuration & Identity (FR31–33), and Backend Event Ingestion (FR34–40). All 40 FRs must be implemented; no optional FRs in MVP.

**Non-Functional Requirements:** 20 NFRs across Performance, Security, Reliability, and Integration. Architecturally load-bearing NFRs:
- NFR5: Display loop never blocked by network I/O → requires task separation
- NFR11: MQTT reconnect ≤30s → requires async reconnect with exponential backoff
- NFR13: Alarm triggers without network → local evaluation against NTP-synced RTC
- NFR7: TLS 1.2+ on ESP32 → memory budget constraint for mbedTLS
- NFR18: MQTT topic contract is versioned → breaking changes require coordination

**Scale & Complexity:**
- Primary domains: Embedded firmware (C++/Arduino/FreeRTOS) + Node.js backend
- Complexity level: Medium-High
- Estimated architectural components: 8–10 (firmware tasks, state machines, MQTT contract, NVS layer, server ingestion module, config management)

### Technical Constraints & Dependencies

- **Runtime:** ESP32 dual-core; FreeRTOS available; ~520KB SRAM, ~4MB flash
- **Toolchain:** PlatformIO exclusively; all libraries must be PlatformIO-registry compatible
- **MQTT contract:** Defined in `docs/lcd-reference.md`; treated as versioned API
- **Brownfield:** `clock-server` is an existing Node.js service; architecture must extend it non-destructively
- **No OTA in MVP:** Firmware updates via USB flash only; architecture must not require OTA for basic operation
- **Credentials:** Never in source; firmware via PlatformIO build env; server via env vars

### Cross-Cutting Concerns

- **MQTT contract integrity:** Topic structure, QoS, and retained flags are load-bearing across firmware, `clock-server`, and third-party publishers
- **`deviceId` scoping:** All subscriptions, publications, and payload validation are scoped to device identity — both firmware and server must apply this consistently
- **Error observability:** `command_result` events are the diagnostic contract; both systems must honour the status vocabulary (`received`, `applied`, `rejected`, `failed`)
- **Credential hygiene:** Both systems must load secrets from environment; no hardcoding
- **Non-blocking I/O:** Network operations (MQTT, NTP) must never stall the display loop; applies to firmware only but shapes the entire task architecture

## Starter Template Evaluation

### Primary Technology Domain

Dual-system IoT project: embedded firmware (C++/Arduino/FreeRTOS on ESP32) + Node.js backend service. Neither system requires a starter template — both are already initialized.

### Firmware Foundation (Already Established)

The PlatformIO project is initialized and the hardware bring-up is complete. The following decisions are already made and confirmed:

**Board & Runtime:**
- Platform: `espressif32`, Board: `esp32dev`, Framework: Arduino
- CPU: ESP32 dual-core Xtensa LX6 @ up to 240MHz, ~520KB SRAM, ~4MB flash

**Display:**
- Driver: ILI9341 (SPI), 240×320 portrait / 320×240 landscape
- Library: `TFT_eSPI @ ^2.5.43` (PlatformIO, bodmer)
- SPI bus: VSPI (CS=15, DC=2, SCK=14, MOSI=13, MISO=12, BL=21)

**Touch:**
- IC: XPT2046 (resistive touch, confirmed from bring-up code)
- SPI bus: separate from display (SCK=25, MOSI=32, MISO=39, CS=33, IRQ=36)
- Library: TBD — verify whether TFT_eSPI built-in touch supports separate SPI bus; fallback to `XPT2046_Touchscreen` library

**RGB LED:** Common-anode, active-low (GPIO 22=R, 16=G, 17=B)

**Audio:** ESP32 DAC on GPIO26 + active-low amplifier enable on GPIO4

**Initialization:** Project already initialized. First implementation story begins with the display state machine on top of the existing TFT_eSPI scaffold.

### Backend Foundation (Brownfield)

`clock-server` is an existing Node.js service with MQTT publish capabilities. Architecture extends it non-destructively with an event ingestion module. No starter template; implementation begins by adding the subscriber/ingestion layer to the existing service.

## Core Architectural Decisions

### Decision Priority Analysis

**Critical Decisions (Block Implementation):**
- Firmware task architecture (NFR5 compliance)
- MQTT library selection (TLS + reconnect strategy)
- Touch library selection (separate SPI bus requirement)
- Secrets management approach (NFR8/NFR10 compliance)

**Important Decisions (Shape Architecture):**
- Audio generation method
- Clock-server ingestion architecture and storage strategy

**Deferred Decisions (Post-MVP):**
- OTA update mechanism
- Backend HTTP APIs for device state read-back
- Alarm persistence in NVS

### Firmware Task Architecture

**Decision:** Dual FreeRTOS task model — `DisplayTask` on Core 1, `NetworkTask` on Core 0.

| Task | Core | Priority | Responsibilities |
|---|---|---|---|
| `DisplayTask` | Core 1 (app core) | High | TFT refresh, display state machine, touch polling, RGB LED, audio, alarm evaluation |
| `NetworkTask` | Core 0 (proto core) | Normal | Wi-Fi management, MQTT connect/reconnect, NTP sync, inbound message receipt |

**Inter-task communication:** FreeRTOS queues. `NetworkTask` pushes parsed command structs onto a `commandQueue`; `DisplayTask` drains the queue each tick. State updates from display to network (events to publish) via a separate `eventQueue`.

**Rationale:** Satisfies NFR5 (display loop never blocked by network I/O). Alarm evaluation runs inside `DisplayTask` against RTC time — no network dependency at trigger time (NFR13). Each task owns its resources exclusively; no mutex needed for display or MQTT client.

### MQTT Library & TLS

**Decision:** `PubSubClient` + `WiFiClientSecure` (ESP32 Arduino built-in).

- PubSubClient handles MQTT 3.1/3.1.1; called from `NetworkTask` in a synchronous loop
- `WiFiClientSecure` provides mbedTLS-backed TLS 1.2; supports CA certificate validation or insecure mode for local dev
- Manual reconnect loop in `NetworkTask` with exponential backoff; resubscribes after reconnect (FR24, NFR11)
- QoS 0 and 1 supported; retained flag set per topic per `docs/lcd-reference.md` delivery rules

**Version:** `PubSubClient @ ^2.8` (knolleary, PlatformIO registry)

**Rationale:** Synchronous model fits naturally with the `NetworkTask` loop. Simpler to reason about than async callbacks across task boundaries.

### Touch Library

**Decision:** `XPT2046_Touchscreen` (PaulStoffregen).

- XPT2046 uses a dedicated SPI bus (SCK=25, MOSI=32, MISO=39, CS=33, IRQ=36) — separate from the ILI9341 display SPI
- `XPT2046_Touchscreen` natively supports a second `SPIClass` instance (HSPI on ESP32)
- TFT_eSPI built-in touch not used — assumes shared SPI bus
- Touch polling runs inside `DisplayTask`; IRQ pin (GPIO36) used for interrupt-driven detection

**Version:** `XPT2046_Touchscreen @ ^1.4` (PlatformIO registry)

### Audio Generation

**Decision:** `ledcWriteTone()` (ESP32 Arduino LEDC peripheral) via the amplifier.

- LEDC PWM output to amplifier input (GPIO26); amplifier enable on GPIO4 (active-low: `LOW` = enabled, `HIGH` = muted)
- `ledcSetup()` + `ledcAttachPin()` + `ledcWriteTone()` — no additional library
- Alarm tone pattern (e.g. 880Hz beep, 200ms on/200ms off) managed in `DisplayTask` alarm ringing state
- Amplifier muted during normal operation; enabled only when alarm is ringing

### Clock-Server Event Ingestion Architecture

**Decision:** Same-process Node.js module; structured console logging only for MVP.

- New `DeviceEventSubscriber` module added to `clock-server` alongside existing MQTT publisher
- Subscribes to `clocks/+/events/#` and `clocks/+/state/#` wildcard topics
- Dispatches received events to typed handlers (one per event type: presence, heartbeat, alarm_state, display_state, alarm_triggered, alarm_acknowledged, command_result)
- **MVP storage:** Structured JSON console logging only — sufficient for diagnostics
- **Growth:** SQLite or existing DB integration for persistent event history

**Rationale:** Non-destructive brownfield extension. Handlers are independently testable. Wildcard subscriptions support additional device instances automatically.

### Configuration & Secrets Management

**Firmware:**
- `include/credentials.h` (gitignored) — `constexpr` strings for Wi-Fi SSID/password, MQTT broker URL/port/user/password
- `include/device_config.h` (tracked) — `constexpr` strings for `DEVICE_ID`, `NTP_SERVER`, `MQTT_TOPIC_PREFIX`
- `include/credentials.h.example` (tracked) — template with placeholder values

**Backend:**
- `.env` file (gitignored) loaded via `dotenv`
- Variables: `MQTT_BROKER_URL`, `MQTT_USERNAME`, `MQTT_PASSWORD`

### Decision Impact Analysis

**Implementation Sequence:**
1. `credentials.h.example` + `device_config.h` scaffolding
2. `NetworkTask` — Wi-Fi + MQTT connect/reconnect with PubSubClient + WiFiClientSecure
3. `DisplayTask` — TFT_eSPI display loop + display state machine scaffold
4. FreeRTOS `commandQueue` and `eventQueue` type definitions
5. NTP sync in `NetworkTask`; RTC-based alarm evaluation in `DisplayTask`
6. Command parsing and queue dispatch
7. Display state machine: clock face → message overlay → alarm ringing
8. XPT2046 touch integration for alarm dismiss
9. LEDC audio for alarm tone + amplifier enable
10. NVS persistence for brightness (ESP32 Preferences library)
11. `clock-server`: `DeviceEventSubscriber` module

**Cross-Component Dependencies:**
- Queue message types must be defined before either task is implemented
- `credentials.h.example` must exist before any developer can build
- MQTT topic constants must match `docs/lcd-reference.md` exactly — single source of truth; consider extracting to a shared `mqtt_topics.h`

## Implementation Patterns & Consistency Rules

### Naming Patterns

**Firmware (C++):**
- Files: `snake_case.h` / `snake_case.cpp` (e.g. `display_task.h`, `mqtt_topics.h`)
- Classes: `PascalCase` (e.g. `DisplayTask`, `AlarmState`)
- Methods: `camelCase` (e.g. `handleCommand()`, `updateBrightness()`)
- Constants: `kPascalCase` — follow existing `board_config.h` convention (e.g. `kLedRed`, `kAlarmRinging`)
- FreeRTOS queues: `camelCase` + `Queue` suffix (e.g. `commandQueue`, `eventQueue`)
- Namespaces: `snake_case` (e.g. `namespace display`, `namespace mqtt`)
- Enums: `PascalCase` type, `kPascalCase` values (e.g. `enum class DisplayMode { kClock, kMessage, kAlarmRinging, kError }`)

**MQTT Payloads (shared contract — both systems must match):**
- JSON field names: `camelCase` (matches lcd-reference.md: `deviceId`, `alarmTime`, `durationSeconds`)
- Timestamps: RFC 3339 UTC strings (e.g. `"2030-01-01T06:00:00Z"`)
- Boolean values: `true`/`false` (JSON native)

### Structure Patterns

**Firmware source layout:**
```
src/
  main.cpp                 — FreeRTOS task launch only; no logic
  display_task.cpp/.h      — DisplayTask implementation
  network_task.cpp/.h      — NetworkTask implementation
  display_state.h          — DisplayMode enum + state struct
  alarm_manager.cpp/.h     — Alarm storage, evaluation, trigger logic
  mqtt_handler.cpp/.h      — Command parsing, event publishing helpers
  ntp_client.cpp/.h        — NTP sync wrapper
  brightness.cpp/.h        — Brightness control + NVS persistence
include/
  board_config.h           — GPIO constants (existing, tracked)
  device_config.h          — DEVICE_ID, NTP_SERVER, topic prefix (tracked)
  credentials.h            — Wi-Fi + MQTT secrets (gitignored)
  credentials.h.example    — Template (tracked)
  mqtt_topics.h            — All topic string constants (tracked, single source of truth)
```

**No logic in `main.cpp`** — it only calls `xTaskCreatePinnedToCore()` for each task and returns.

**Backend (clock-server extension):**
```
src/
  mqtt/
    subscriber.ts          — DeviceEventSubscriber: subscribe + dispatch
    handlers/
      presence.ts
      heartbeat.ts
      alarmState.ts
      displayState.ts
      alarmTriggered.ts
      alarmAcknowledged.ts
      commandResult.ts
```

### Format Patterns

**MQTT command parsing (firmware):**
- Parse JSON with ArduinoJson; validate `deviceId` before any other field
- On parse error: publish `command_result` with `status: "failed"`, `detail: "json_parse_error"`
- On `deviceId` mismatch: publish `command_result` with `status: "rejected"`, `detail: "device_id_mismatch"`
- On success: publish `command_result` with `status: "applied"`

**`command_result` status vocabulary** (both systems must use exactly these strings):
- `"received"` — command received, not yet processed
- `"applied"` — command processed successfully
- `"rejected"` — command refused (invalid payload, wrong deviceId)
- `"failed"` — command attempted but hardware/system error

**Event payloads:** Always include `deviceId` and `at` (RFC 3339 timestamp) fields.

### Communication Patterns

**FreeRTOS queue message types:**
```cpp
struct CommandMessage {
  enum class Type { kSetAlarm, kDisplayMessage, kSetBrightness };
  Type type;
  // command-specific fields per type
};

struct EventMessage {
  enum class Type { kAlarmTriggered, kAlarmAcknowledged, kCommandResult, kDisplayState, kAlarmState };
  Type type;
};
```
- `commandQueue`: `NetworkTask` → `DisplayTask` (capacity: 8)
- `eventQueue`: `DisplayTask` → `NetworkTask` (capacity: 16)
- Sender: `xQueueSend()` with `portMAX_DELAY`; receiver: `xQueueReceive()` with tick timeout

**MQTT topic constants:** Defined once in `mqtt_topics.h`; topics constructed at runtime via `snprintf` using `DEVICE_ID` from `device_config.h`. No magic strings elsewhere.

### Process Patterns

**Display state machine — enforced priority:**
```
alarm_ringing > message > clock > error > time_unavailable
```
A lower-priority state cannot override a higher-priority active state. Any code changing display state must check current priority.

**Alarm evaluation rule (runs every second in DisplayTask):**
- Fires when: `ntpSynced == true` AND `currentTime >= alarmTime` AND `alarmArmed == true`
- After trigger: set `alarmArmed = false` immediately (prevents re-trigger)

**Error handling (firmware):**
- MQTT publish failure: log to Serial, do not crash; retry next loop
- JSON parse error: always publish `command_result status: "failed"`; never silently discard
- NTP failure: set `ntpSynced = false`, show time-unavailable indicator, block alarm evaluation

**Error handling (backend):**
- Malformed payload: log structured JSON `{ topic, error, rawPayload }`; do not throw
- Unknown event type: log and discard

### Enforcement Guidelines

**Firmware agents MUST:**
- Use `kPascalCase` for all constants; no `ALL_CAPS` macros except PlatformIO build defines
- Never call blocking network functions from `DisplayTask`
- Never access the `tft` instance from `NetworkTask`
- Always validate `deviceId` before processing any command
- Always publish `command_result` for every received command
- Mute amplifier (`HIGH` on GPIO4) before stopping tone via `ledcWriteTone(channel, 0)`

**Backend agents MUST:**
- Never add MQTT subscriptions outside `DeviceEventSubscriber`
- Use exact status strings from the `command_result` vocabulary
- Log all events as structured JSON, not plain text
- Never hardcode topic strings — use topic pattern constants

## Project Structure & Boundaries

### FR Category → Component Mapping

| FR Category | Firmware Component | Backend Component |
|---|---|---|
| Time & Display (FR1–6) | `display_task`, `display_state`, `ntp_client` | — |
| MQTT Command Handling (FR7–14) | `mqtt_handler`, `network_task` | `subscriber.ts`, `handlers/commandResult.ts` |
| Alarm (FR15–21) | `alarm_manager`, `display_task` | `handlers/alarmTriggered.ts`, `handlers/alarmAcknowledged.ts`, `handlers/alarmState.ts` |
| Connectivity & Resilience (FR22–27) | `network_task` | `subscriber.ts` (reconnect) |
| Observability & Diagnostics (FR28–30) | `mqtt_handler` (publishes events) | all `handlers/` |
| Configuration & Identity (FR31–33) | `device_config.h`, `credentials.h`, `brightness` | `.env`, `subscriber.ts` |
| Backend Event Ingestion (FR34–40) | — (publishes events) | `subscriber.ts`, all `handlers/` |

### Complete Project Directory Structure

```
mqtt-smart-clock/                        ← repo root
│
├── platformio.ini                       ← PlatformIO project (existing)
├── .gitignore                           ← excludes credentials.h, .env, .pio/
├── README.md
│
├── include/                             ← Firmware headers (tracked)
│   ├── board_config.h                   ← GPIO pin constants (existing, FR31)
│   ├── device_config.h                  ← DEVICE_ID, NTP_SERVER, topic prefix (FR32–33)
│   ├── mqtt_topics.h                    ← All MQTT topic string constants (FR7–14, FR28–30)
│   ├── credentials.h                    ← Wi-Fi + MQTT secrets (gitignored, FR22, NFR8)
│   └── credentials.h.example           ← Template with placeholders (tracked)
│
├── src/                                 ← Firmware source
│   ├── main.cpp                         ← FreeRTOS task launch only; no logic (FR22)
│   ├── display_task.cpp                 ← DisplayTask: TFT loop, state machine, touch,
│   ├── display_task.h                   │  alarm evaluation, audio, LED (FR1–6, FR15–21)
│   ├── display_state.h                  ← DisplayMode enum + DisplayState struct (FR1, FR5)
│   ├── network_task.cpp                 ← NetworkTask: Wi-Fi, MQTT, NTP sync (FR22–27)
│   ├── network_task.h                   │
│   ├── alarm_manager.cpp                ← Alarm storage, evaluation, trigger logic (FR15–21)
│   ├── alarm_manager.h                  │
│   ├── mqtt_handler.cpp                 ← Command parsing, event publishing helpers (FR7–14, FR28–30)
│   ├── mqtt_handler.h                   │
│   ├── ntp_client.cpp                   ← NTP sync wrapper via configTime()/sntp (FR3, FR4)
│   ├── ntp_client.h                     │
│   ├── brightness.cpp                   ← Brightness control + NVS persistence (FR12, FR33)
│   └── brightness.h                     │
│
├── test/                                ← Native unit tests (PlatformIO test runner)
│   ├── test_alarm_manager/
│   │   └── test_alarm_evaluation.cpp   ← Alarm trigger logic (FR16, FR17)
│   └── test_mqtt_handler/
│       └── test_command_parsing.cpp    ← JSON parse + deviceId validation (FR8, FR9)
│
├── docs/
│   └── lcd-reference.md                ← MQTT contract (source of truth; existing)
│
└── clock-server/                        ← Existing Node.js service (brownfield)
    ├── package.json                     ← Existing; add mqtt dependency if not present
    ├── .env                             ← Gitignored: MQTT_BROKER_URL/USERNAME/PASSWORD
    ├── .env.example                     ← Tracked template
    ├── tsconfig.json                    ← Existing TypeScript config
    │
    └── src/
        ├── (existing files...)          ← Existing publisher; not modified
        └── mqtt/
            ├── subscriber.ts            ← DeviceEventSubscriber: subscribe + dispatch (FR34–37)
            ├── topic_patterns.ts        ← Wildcard topic constants (never inline strings)
            └── handlers/
                ├── presence.ts          ← clocks/+/events/+/presence (FR38)
                ├── heartbeat.ts         ← clocks/+/events/+/heartbeat (FR38)
                ├── alarmState.ts        ← clocks/+/state/+/alarm (FR39)
                ├── displayState.ts      ← clocks/+/state/+/display (FR39)
                ├── alarmTriggered.ts    ← clocks/+/events/+/alarm_triggered (FR40)
                ├── alarmAcknowledged.ts ← clocks/+/events/+/alarm_acknowledged (FR40)
                └── commandResult.ts     ← clocks/+/events/+/command_result (FR40)
```

### Architectural Boundaries

**FreeRTOS Queue Boundary (firmware-internal):**
- `commandQueue` (capacity 8): `NetworkTask` → `DisplayTask`. Carries parsed `CommandMessage` structs (set_alarm, display_message, set_brightness). `NetworkTask` owns enqueue; `DisplayTask` owns dequeue.
- `eventQueue` (capacity 16): `DisplayTask` → `NetworkTask`. Carries `EventMessage` structs (alarm_triggered, alarm_acknowledged, command_result, display_state, alarm_state). `DisplayTask` owns enqueue; `NetworkTask` owns dequeue and MQTT publish.
- **Rule:** No direct function calls across task boundary. All cross-task data must transit a queue. Violation of this rule breaks NFR5.

**MQTT Topic Boundary (firmware ↔ world):**
- All topic strings are constants in `mqtt_topics.h` (firmware) and `topic_patterns.ts` (clock-server). No inline topic strings anywhere else.
- Commands inbound: `clocks/commands/{deviceId}/set_alarm`, `.../display_message`, `.../set_brightness`
- Events outbound: `clocks/events/{deviceId}/presence`, `.../heartbeat`, `.../command_result`, `.../alarm_triggered`, `.../alarm_acknowledged`
- State outbound (retained): `clocks/state/{deviceId}/display`, `.../alarm`
- `deviceId` validation is mandatory before any command processing (FR8, NFR18).

**NVS Boundary (brightness persistence):**
- `brightness.cpp` is the sole owner of `Preferences` NVS reads/writes. No other file calls `Preferences` directly.
- `DisplayTask` calls `brightness::setBrightness()` and `brightness::getBrightness()` only.

**Clock-Server Module Boundary:**
- All MQTT subscriptions are created exclusively inside `DeviceEventSubscriber` (`subscriber.ts`). No other module in `clock-server` subscribes to MQTT topics.
- Event handlers in `handlers/` are pure functions: receive parsed payload, return void, emit structured log. No side effects beyond logging in MVP.

### Requirements → File Mapping

| Requirement | Primary File(s) |
|---|---|
| FR1 — Clock face display | `display_task.cpp`, `display_state.h` |
| FR2 — Time format toggle | `display_task.cpp`, `device_config.h` |
| FR3 — Display message overlay | `display_task.cpp`, `mqtt_handler.cpp` |
| FR4 — Message auto-dismiss | `display_task.cpp` (timer logic) |
| FR5 — Display state machine | `display_task.cpp`, `display_state.h` |
| FR6 — Brightness control | `brightness.cpp`, `display_task.cpp` |
| FR7 — set_alarm command receive | `network_task.cpp`, `mqtt_handler.cpp` |
| FR8 — deviceId validation | `mqtt_handler.cpp` |
| FR9 — JSON parse error handling | `mqtt_handler.cpp` |
| FR10 — display_message receive | `network_task.cpp`, `mqtt_handler.cpp` |
| FR11 — set_brightness receive | `network_task.cpp`, `mqtt_handler.cpp` |
| FR12 — Brightness NVS persist | `brightness.cpp` |
| FR13 — command_result publish | `mqtt_handler.cpp`, `network_task.cpp` |
| FR14 — QoS/retained per topic | `mqtt_topics.h`, `network_task.cpp` |
| FR15 — Alarm storage | `alarm_manager.cpp` |
| FR16 — Alarm trigger evaluation | `alarm_manager.cpp`, `display_task.cpp` |
| FR17 — Alarm ringing state | `display_task.cpp`, `display_state.h` |
| FR18 — Alarm dismiss (touch) | `display_task.cpp`, `alarm_manager.cpp` |
| FR19 — alarm_triggered publish | `alarm_manager.cpp`, `network_task.cpp` |
| FR20 — alarm_acknowledged publish | `alarm_manager.cpp`, `network_task.cpp` |
| FR21 — Alarm audio tone | `display_task.cpp` (LEDC calls) |
| FR22 — Wi-Fi connect | `network_task.cpp`, `credentials.h` |
| FR23 — MQTT connect/auth | `network_task.cpp`, `credentials.h` |
| FR24 — MQTT reconnect + backoff | `network_task.cpp` |
| FR25 — NTP sync | `ntp_client.cpp`, `network_task.cpp` |
| FR26 — time_unavailable indicator | `display_task.cpp`, `display_state.h` |
| FR27 — Presence/heartbeat publish | `network_task.cpp`, `mqtt_handler.cpp` |
| FR28–30 — Observability events | `mqtt_handler.cpp`, `network_task.cpp` |
| FR31 — Board identity constants | `board_config.h` |
| FR32 — Device identity constants | `device_config.h` |
| FR33 — Credentials gitignored | `credentials.h`, `.gitignore` |
| FR34 — Subscribe to device events | `clock-server/src/mqtt/subscriber.ts` |
| FR35 — Dispatch to handlers | `clock-server/src/mqtt/subscriber.ts` |
| FR36 — command_result ingestion | `clock-server/src/mqtt/handlers/commandResult.ts` |
| FR37 — deviceId scoping | `clock-server/src/mqtt/subscriber.ts` |
| FR38 — Presence/heartbeat ingestion | `handlers/presence.ts`, `handlers/heartbeat.ts` |
| FR39 — State ingestion | `handlers/alarmState.ts`, `handlers/displayState.ts` |
| FR40 — Alarm event ingestion | `handlers/alarmTriggered.ts`, `handlers/alarmAcknowledged.ts` |

### Data Flow

```
MQTT Broker
    │
    │  clocks/commands/{deviceId}/set_alarm  (QoS 1)
    ▼
NetworkTask ──[commandQueue]──▶ DisplayTask
    │                               │
    │                         alarm_manager
    │                               │
    │                         [eventQueue]
    │                               │
    ◀───────────────────────────────┘
    │  clocks/events/{deviceId}/alarm_triggered  (QoS 0)
    │  clocks/events/{deviceId}/command_result   (QoS 1)
    │  clocks/state/{deviceId}/alarm             (QoS 1, retained)
    ▼
MQTT Broker
    │
    ▼
DeviceEventSubscriber (clock-server)
    ├──▶ handlers/alarmTriggered.ts  → structured JSON log
    ├──▶ handlers/commandResult.ts   → structured JSON log
    └──▶ handlers/alarmState.ts      → structured JSON log
```

### Development Workflow

**Firmware build:** `pio run -e hosyond_28_esp32` — compiles all `src/` + `include/` files. `credentials.h` must exist (copy from `.example`) before first build.

**Firmware test:** `pio test -e native` — runs `test/` suites on host (no hardware required for unit tests of pure-logic modules like `alarm_manager` and `mqtt_handler`).

**Backend dev:** `cd clock-server && npm install && npm run dev` — starts existing service with `DeviceEventSubscriber` attached. Requires `.env` with broker credentials.

**Secrets bootstrap:**
```bash
# Firmware
cp include/credentials.h.example include/credentials.h
# edit credentials.h with real values

# Backend
cp clock-server/.env.example clock-server/.env
# edit .env with real values
```

## Architecture Validation Results

### Coherence Validation ✅

**Decision Compatibility:**
- `TFT_eSPI` (VSPI: SCK=14, MOSI=13, MISO=12, CS=15) and `XPT2046_Touchscreen` (HSPI: SCK=25, MOSI=32, MISO=39, CS=33) use independent `SPIClass` instances — no bus contention. ✅
- `PubSubClient` synchronous model is called exclusively from `NetworkTask` loop — no reentrancy risk. ✅
- LEDC PWM (GPIO26) does not share pins with any SPI/I2C peripheral. ✅
- Amplifier enable (GPIO4, active-low) and DAC output (GPIO26) are independent GPIO operations manageable within `DisplayTask`. ✅
- `Preferences` NVS access is isolated to `brightness.cpp`; called from `DisplayTask` only during setup/brightness-change events — no concurrent NVS access. ✅
- All decisions are compatible with PlatformIO + Arduino framework on ESP32. ✅

**Pattern Consistency:**
- `kPascalCase` constants used consistently across all headers. ✅
- `camelCase` queue names (`commandQueue`, `eventQueue`) match FreeRTOS queue naming pattern. ✅
- MQTT JSON field names (`camelCase`) match `lcd-reference.md` contract. ✅
- `command_result` status vocabulary identical in firmware patterns and backend handler spec. ✅
- Display state priority order (`alarm_ringing > message > clock > error > time_unavailable`) is coherent with alarm evaluation rule (trigger → immediate state escalation). ✅

**Structure Alignment:**
- Dual-task boundary is enforced structurally: `tft` instance lives in `display_task.cpp`; MQTT client lives in `network_task.cpp`. File ownership makes cross-task access a compile-time visibility violation. ✅
- `mqtt_topics.h` as single source of truth maps cleanly onto the single-file import model in both firmware and `topic_patterns.ts` in the backend. ✅

### Requirements Coverage Validation ✅

**Functional Requirements (40/40 covered):**
All 40 FRs are mapped to specific files in the Project Structure section. Cross-checked:
- FR1–6 (Time & Display): `display_task.cpp`, `display_state.h`, `ntp_client.cpp` ✅
- FR7–14 (MQTT Commands): `mqtt_handler.cpp`, `network_task.cpp`, `mqtt_topics.h` ✅
- FR15–21 (Alarm): `alarm_manager.cpp`, `display_task.cpp` ✅
- FR22–27 (Connectivity): `network_task.cpp`, `credentials.h` ✅
- FR28–30 (Observability): `mqtt_handler.cpp` (publishes), `handlers/` (ingests) ✅
- FR31–33 (Config/Identity): `board_config.h`, `device_config.h`, `credentials.h` ✅
- FR34–40 (Backend Ingestion): `subscriber.ts`, all seven `handlers/` ✅

**Architecturally Load-Bearing NFRs:**
- NFR5 (display never blocked): Dual FreeRTOS task model; `NetworkTask` on Core 0, `DisplayTask` on Core 1; cross-task data via queues only. ✅
- NFR11 (MQTT reconnect ≤30s): Exponential backoff loop in `NetworkTask`; resubscribes after reconnect. ✅
- NFR13 (alarm triggers without network): Alarm evaluation runs in `DisplayTask` against RTC time; no network call at trigger time. ✅
- NFR7 (TLS 1.2+): `WiFiClientSecure` + mbedTLS on ESP32; CA cert validation or insecure mode for local dev. ✅
- NFR18 (versioned MQTT contract): All topic strings in `mqtt_topics.h` + `topic_patterns.ts`; no inline strings; breaking changes require coordinated header update. ✅
- NFR3 (message display ≤2s): `DisplayTask` runs at high priority on Core 1; `commandQueue` drained each tick; no blocking I/O in display loop. ✅
- NFR4 (alarm trigger ±1s): Alarm evaluation runs every second in `DisplayTask`; `ntp_client` syncs system clock to NTP. ✅
- NFR8/NFR10 (credentials not in source): `credentials.h` gitignored; `.env` gitignored; `.example` files tracked. ✅

### Implementation Readiness Validation ✅

**Decision Completeness:**
- All critical library choices include version pins (PubSubClient @^2.8, TFT_eSPI @^2.5.43, XPT2046_Touchscreen @^1.4). ✅
- GPIO assignments are complete and sourced from existing `board_config.h`. ✅
- FreeRTOS queue types (`CommandMessage`, `EventMessage`) defined with capacities. ✅
- MQTT status vocabulary is exhaustive and exact. ✅

**Structure Completeness:**
- Every file in the source layout has an annotated responsibility. ✅
- All 7 backend handler files are named and topic-mapped. ✅
- Integration boundaries (queue, MQTT, NVS, module) are all explicitly bounded. ✅

**Pattern Completeness:**
- Naming conventions cover files, classes, methods, constants, enums, namespaces, queues. ✅
- Error handling patterns cover firmware (MQTT publish failure, JSON parse error, NTP failure) and backend (malformed payload, unknown event type). ✅
- Display state machine priority order is explicit. ✅
- Alarm re-trigger prevention rule is explicit (`alarmArmed = false` immediately after trigger). ✅

### Gap Analysis Results

**Important Gaps (non-blocking; address before first build):**

1. **ArduinoJson version not pinned** — `mqtt_handler.cpp` uses ArduinoJson for all JSON parsing, but no version is specified in `platformio.ini` lib_deps. ArduinoJson v6 and v7 have different APIs. Recommend: `bblanchon/ArduinoJson @ ^7.0` in `platformio.ini`.

2. **Native test environment not defined** — `test/` directory is specified in the project structure, but `platformio.ini` has no `[env:native]` section. Recommend adding:
   ```ini
   [env:native]
   platform = native
   lib_deps =
     bblanchon/ArduinoJson @ ^7.0
   ```

3. **`snprintf` topic construction pattern not illustrated** — `mqtt_topics.h` specifies runtime topic construction via `snprintf` using `DEVICE_ID`, but no concrete example is documented. Implementors should add a usage comment to `mqtt_topics.h`.

**Deferred Gaps (post-MVP, documented):**
- OTA update mechanism ✅
- Backend HTTP APIs for device state read-back ✅
- Alarm persistence in NVS ✅
- SQLite/DB event history in clock-server ✅

### Architecture Completeness Checklist

**✅ Requirements Analysis**
- [x] Project context thoroughly analyzed (dual-system IoT, 40 FRs, 20 NFRs)
- [x] Scale and complexity assessed (Medium-High, 8–10 architectural components)
- [x] Technical constraints identified (ESP32 SRAM budget, PlatformIO-only, brownfield backend)
- [x] Cross-cutting concerns mapped (MQTT contract, deviceId scoping, credential hygiene, non-blocking I/O)

**✅ Architectural Decisions**
- [x] Critical decisions documented with versions (PubSubClient @^2.8, XPT2046_Touchscreen @^1.4, TFT_eSPI @^2.5.43)
- [x] Technology stack fully specified (ESP32 Arduino/FreeRTOS + Node.js TypeScript)
- [x] Integration patterns defined (FreeRTOS queues, MQTT topics, NVS, same-process module)
- [x] Performance considerations addressed (dual-core task isolation, queue capacities)

**✅ Implementation Patterns**
- [x] Naming conventions established (kPascalCase, camelCase, snake_case per entity type)
- [x] Structure patterns defined (display state machine priority, alarm evaluation rule)
- [x] Communication patterns specified (queue ownership, MQTT QoS/retained rules)
- [x] Process patterns documented (error handling, command_result status vocabulary)

**✅ Project Structure**
- [x] Complete directory structure defined (firmware + clock-server)
- [x] Component boundaries established (4 explicit boundaries: queue, MQTT, NVS, module)
- [x] Integration points mapped (data flow diagram)
- [x] Requirements-to-structure mapping complete (all 40 FRs)

### Architecture Readiness Assessment

**Overall Status: READY FOR IMPLEMENTATION**

**Confidence Level: High** — all decisions are unambiguous, all FRs have file ownership, all boundaries prevent accidental coupling.

**Key Strengths:**
- Non-blocking display loop is structurally guaranteed by task separation — NFR5 cannot be violated without a compile-time visibility error
- MQTT topic contract is a single source of truth in one header file; drift is impossible without a deliberate change
- Brownfield extension is fully non-destructive — existing `clock-server` code is untouched
- Alarm evaluation has no network dependency at trigger time — works through broker outage
- All secrets paths are gitignored by design; no accidental credential commit possible

**Areas for Future Enhancement:**
- ArduinoJson version pinning (important gap — address before build)
- Native PlatformIO test environment definition
- Post-MVP: NVS alarm persistence, backend SQLite event history, HTTP state read-back API, OTA

### Implementation Handoff

**AI Agent Guidelines:**
- Follow all architectural decisions exactly as documented; do not introduce new libraries without updating `platformio.ini` and this document
- Implement queue types (`CommandMessage`, `EventMessage`) before either task — they are the shared contract
- Use implementation patterns consistently: `kPascalCase` constants, `camelCase` methods, no blocking calls from `DisplayTask`
- Validate `deviceId` before processing any command — this is both a functional requirement (FR8) and a security boundary
- Always publish `command_result` for every received command, even on error

**First Implementation Priority:**
1. Resolve ArduinoJson gap — add `bblanchon/ArduinoJson @ ^7.0` to `platformio.ini`
2. Create `include/credentials.h.example`, `include/device_config.h`, `include/mqtt_topics.h` scaffolds
3. Define `CommandMessage` and `EventMessage` struct types (shared contract)
4. Implement `NetworkTask` (Wi-Fi + MQTT connect/reconnect + NTP)
5. Implement `DisplayTask` scaffold (display loop + state machine + queue drain)
