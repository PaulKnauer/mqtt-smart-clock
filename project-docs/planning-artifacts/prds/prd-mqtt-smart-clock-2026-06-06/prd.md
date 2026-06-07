---
title: "PRD: mqtt-smart-clock"
status: draft
created: 2026-06-06
updated: 2026-06-06
---

# PRD: mqtt-smart-clock

## 0. Document Purpose

This PRD defines the first product version of `mqtt-smart-clock`, ESP32 firmware for the Hosyond 2.8" TFT display that turns it into a connected smart clock. It is written for architecture and implementation follow-up. Features are grouped by capability, functional requirements use stable `FR-N` IDs.

## 1. Vision

`mqtt-smart-clock` is a connected desk clock that receives commands over MQTT from `clock-server` and `mqtt-mcp-server` — setting alarms, displaying messages, and adjusting brightness. It shows an attractive clock face with NTP-synced time, responds instantly to touch, and never silently dies to a network drop.

The firmware is the display endpoint in a complete IoT pipeline: AI agent → MCP server → MQTT broker → ESP32 smart clock. It runs on a dual-core FreeRTOS architecture with strict task isolation for reliability.

## 2. Target User

### 2.1 Primary Persona

**Paul, the home-lab owner**, has a Hosyond 2.8" ESP32 display that he wants to function as a desk clock. He wants to set alarms and display messages from his AI assistants (via Hermes, Claude Desktop, etc.) without touching the device. He expects it to just work — connect to WiFi, sync time, receive commands.

### 2.2 Secondary Personas

- **Family members:** see the time at a glance, read displayed messages, dismiss alarms.
- **Paul as developer:** needs a maintainable platform with interface-based architecture, native unit testing, and the same PlatformIO patterns as his other ESP projects (MqttDht22, etc.).

### 2.3 Jobs To Be Done

- Display the current time with NTP accuracy, shown on an easy-to-read clock face.
- Receive and display transient messages (reminders, notifications).
- Set, store, and trigger alarms at scheduled times.
- Adjust screen brightness for day/night comfort.
- Dismiss or snooze alarms via touch.
- Signal device health via MQTT heartbeats and presence.

### 2.4 Key User Journeys

- **UJ-1. Paul plugs in the display.** The ESP32 boots, connects to WiFi and MQTT broker, publishes `online` presence, syncs NTP time, and shows the clock face. The whole process takes under 10 seconds.
- **UJ-2. An AI agent sets an alarm.** Paul tells his AI "set an alarm on the kitchen clock for 7am tomorrow." The MCP server publishes to `clocks/commands/clock-1/set_alarm`. The clock receives it, stores the alarm, publishes `command_result: applied`, and shows a brief confirmation on screen.
- **UJ-3. An alarm rings.** At 7:00 the clock plays a buzzer tone, shows "Wake up!" on screen, and pulses the RGB LED. Paul taps the screen to dismiss. The clock publishes `alarm_acknowledged: dismiss`.
- **UJ-4. A message arrives.** An AI says "Meeting in 5 minutes." The clock receives the `display_message` command, shows the message as an overlay for 30 seconds, then returns to the clock face.
- **UJ-5. Nighttime dimming.** A brightness command sets the display to 10% at night. The backlight dims smoothly and the level persists across reboots.

## 3. Glossary

- **DisplayTask** — FreeRTOS task on Core 1 that owns all display rendering, touch polling, alarm evaluation, audio, and RGB LED.
- **NetworkTask** — FreeRTOS task on Core 0 that owns WiFi management, MQTT connect/reconnect/subscribe, NTP sync, and inbound command parsing.
- **Command Queue** — FreeRTOS queue (8 slots) carrying parsed commands from NetworkTask to DisplayTask.
- **Event Queue** — FreeRTOS queue (16 slots) carrying events from DisplayTask to NetworkTask for MQTT publishing.
- **ILI9341** — 240×320 TFT LCD display driver. SPI-connected.
- **XPT2046** — Resistive touch controller. Separate SPI bus.
- **LWT** — MQTT Last Will and Testament. Published automatically when the device disconnects unexpectedly.

## 4. Features

### 4.1 Network Connectivity

**Description:** The device connects to WiFi and MQTT broker on boot and stays connected. Realizes UJ-1.

#### FR-1: WiFi Connection

The device connects to a pre-configured WiFi network on boot.

**Consequences:**
- WiFi credentials are configured at compile time via `credentials.h` / build flags.
- The device retries connection every 5 seconds until successful.
- WiFi status is checked every `loop()` tick.

#### FR-2: MQTT Connection

The device connects to a pre-configured MQTT broker and subscribes to command topics.

**Consequences:**
- MQTT broker URL and port are configured at compile time.
- The device subscribes to `{prefix}/commands/{deviceId}/#` on connect.
- LWT publishes `offline` to `{prefix}/state/{deviceId}/presence` on unexpected disconnect.
- The device publishes `online` to `{prefix}/state/{deviceId}/presence` on first connect (retained).
- Auto-reconnect: retries MQTT connection every 5 seconds if broker drops.

#### FR-3: NTP Time Sync

The device synchronises time from an NTP server after connecting to WiFi.

**Consequences:**
- Default NTP pool: `pool.ntp.org`.
- Timezone offset is configured at compile time.
- Time sync retries if the first attempt fails.

### 4.2 Display

**Description:** The ILI9341 shows a readable clock face and handles display modes. Realizes UJ-1, UJ-4.

#### FR-4: Clock Face

The device displays an attractive clock face with time, date, and day of week.

**Consequences:**
- Time updates every second from the RTC.
- Format: `HH:MM` large digits, `SS` smaller, date below, AM/PM or 24h per config.
- The clock face is the default display mode.

#### FR-5: Display Modes

The device has priority-ordered display modes: alarm > message > clock > error.

**Consequences:**
- Alarm mode takes highest priority — shows alarm label + dismiss/snooze buttons.
- Message mode shows overlay text for `durationSeconds` then returns to previous mode.
- Clock mode is the default idle state.
- Error mode shows when NTP or MQTT is lost (optional).

### 4.3 Commands

**Description:** The device receives and processes MQTT commands. Realizes UJ-2, UJ-4, UJ-5.

#### FR-6: Set Alarm

The device accepts `set_alarm` commands via MQTT.

**Consequences:**
- Parses JSON payload: `deviceId`, `type`, `alarmTime` (RFC3339), optional `label`.
- Validates `deviceId` matches the device's own ID.
- Stores the alarm in NVS / RTC memory (persists across reboots).
- Publishes `command_result: applied` on success.
- Publishes `command_result: rejected` with reason on validation failure.
- Updates retained state topic `{prefix}/state/{id}/alarm`.

#### FR-7: Display Message

The device accepts `display_message` commands via MQTT.

**Consequences:**
- Parses JSON payload: `deviceId`, `type`, `message`, `durationSeconds`.
- Shows the message as an overlay on the display for the specified duration.
- Auto-dismisses after duration, returning to clock face.
- Publishes `command_result: applied` on success.

#### FR-8: Set Brightness

The device accepts `set_brightness` commands via MQTT.

**Consequences:**
- Parses JSON payload: `deviceId`, `type`, `level` (0-100).
- Adjusts backlight PWM duty cycle proportionally.
- Persists brightness level in RTC memory / NVS.
- Publishes `command_result: applied` on success.
- Updates retained state topic `{prefix}/state/{id}/display`.

### 4.4 Alarm Ringing

**Description:** When the scheduled alarm time is reached, the device alerts the user. Realizes UJ-3.

#### FR-9: Trigger Alarm

The device triggers an alarm at the scheduled time.

**Consequences:**
- Activates alarm display mode (highest priority).
- Plays an audible tone via DAC pin (GPIO26).
- Pulses RGB LED in alarm pattern.
- Publishes `alarm_triggered` event with timestamp and label.
- Alarm rings for up to 60 seconds if not dismissed.

#### FR-10: Dismiss / Snooze Alarm

The user can dismiss or snooze an alarm via touch.

**Consequences:**
- Touch screen shows "Dismiss" and "Snooze (5 min)" buttons.
- Dismiss stops the alarm and returns to clock face.
- Snooze re-arms the alarm for 5 minutes.
- Publishes `alarm_acknowledged` event with action type.

### 4.5 Device Events

**Description:** The device reports its state and events for monitoring. Realizes UJ-1.

#### FR-11: Heartbeat

The device publishes a periodic heartbeat event.

**Consequences:**
- Published every 30-60 seconds to `{prefix}/events/{id}/heartbeat`.
- Payload includes: uptime (seconds), WiFi RSSI, NTP sync status, free heap.
- QoS 0, no retain.

#### FR-12: Presence

The device publishes its online/offline presence via retained MQTT state.

**Consequences:**
- `online` published on boot after MQTT connect (retained).
- `offline` published via LWT on unexpected disconnect (retained).
- Topic: `{prefix}/state/{id}/presence`.

## 5. Non-Goals

- No SD card support in v1.
- No OTA firmware updates in v1.
- No web configuration portal.
- No BLE support.
- No MQTT TLS in v1 (plain TCP within home network).
- No advanced alarm patterns (weekly repeats, smart wake).
- No music or complex audio beyond buzzer tones.
- No screen saver or pixel burn-in protection.

## 6. MVP Scope

### 6.1 In Scope

- WiFi + MQTT auto-connecting, auto-reconnecting.
- NTP time sync.
- ILI9341 clock face with HH:MM time display.
- Three MQTT commands: set_alarm, display_message, set_brightness.
- Alarm ringing with buzzer + LED.
- Touch dismiss/snooze for alarms.
- Message overlay with auto-dismiss.
- PWM backlight control with NVS persistence.
- MQTT event publishing: presence, heartbeat, command_result, alarm_triggered, alarm_acknowledged.
- Dual-core FreeRTOS with queue isolation.
- Interface-based architecture for testability.
- Native unit tests for controller logic.

### 6.2 Out of Scope for MVP

- SD card storage or logging.
- OTA updates.
- Web config portal.
- MQTT TLS.
- Advanced alarm scheduling.

## 7. Success Metrics

**Primary**

- **SM-1:** Device boots, connects to WiFi/MQTT, shows time from NTP within 10 seconds.
- **SM-2:** All three MQTT commands are received, validated, and executed correctly.
- **SM-3:** Alarms ring at the correct time and can be dismissed via touch.

**Secondary**

- **SM-4:** Device recovers WiFi and MQTT after network outage without manual reset.
- **SM-5:** Brightness persists across reboots.
- **SM-6:** RTC time is accurate within 1 second of NTP source.

## 8. Assumptions Index

- The MQTT broker is already running on the home network (port 1883, no TLS).
- The device is on the same LAN as the broker (no NAT traversal needed).
- Clock-server and mqtt-mcp-server are already deployed and publishing commands.
- The Hosyond 2.8" display uses the specific pin mapping in `board_config.h`.
