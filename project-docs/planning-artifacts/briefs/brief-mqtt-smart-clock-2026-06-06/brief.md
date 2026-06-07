---
title: "Product Brief: mqtt-smart-clock"
status: draft
created: 2026-06-06
updated: 2026-06-06
---

# Product Brief: mqtt-smart-clock

## Executive Summary

`mqtt-smart-clock` is ESP32 firmware for the **Hosyond 2.8" TFT Display** (ILI9341 + XPT2046 touch) that turns the board into a connected smart clock. It receives commands via MQTT from `clock-server` (the Go HTTP gateway) and `mqtt-mcp-server` (the MCP server), displays the time, renders messages, manages alarms, and reports device state back through MQTT events.

The firmware runs on a dual-core FreeRTOS architecture: **Core 0** handles WiFi/MQTT/network, **Core 1** handles display/touch/alarms. All cross-core communication goes through FreeRTOS message queues — no shared state, no direct cross-core function calls.

## Purpose And Stakes

This is a home-lab IoT project. It completes the smart clock pipeline:

```
LLM / MCP Client → mqtt-mcp-server → MQTT Broker → ESP32 Smart Clock
                    clock-server (HTTP) ────────→ (ILI9341 + touch)
```

The firmware must be reliable (auto-reconnect WiFi/MQTT, no crash loops), responsive (touch feels instant, alarm rings on time), and maintainable (interface-based architecture for testability).

## Primary Users

- **Paul, the home-lab owner:** plugs in the Hosyond display, flashes the firmware, and it appears as a connected clock on his network. He can set alarms, display messages, and adjust brightness from any MCP client.
- **Family members:** see the clock face, read displayed messages, dismiss alarms via touch.

## First-Version Experience

The first version should include:

- **Clock face** — time (from NTP), date, day of week on a clean ILI9341 display
- **MQTT command processing** — subscribe to `clocks/commands/{id}/#`, parse JSON, dispatch to display
- **Display message** — transient overlay for incoming messages with auto-dismiss after duration
- **Set brightness** — PWM control of backlight pin, persisted in RTC/NVS memory
- **Set alarm** — store scheduled alarm, ring at the right time, dismiss/snooze via touch
- **Touch input** — dismiss alarms, snooze, basic interaction
- **MQTT events** — publish presence (LWT), heartbeats, command results, alarm state
- **WiFi/MQTT auto-reconnect** — never needs a power cycle to recover from network drops
- **NTP time sync** — correct time across reboots

## API Contract (from clock-server / mqtt-mcp-server)

### Subscribed Topics

| Topic | Direction | Payload |
|---|---|---|
| `clocks/commands/{id}/set_alarm` | Server→Device | `{"deviceId":"...","type":"set_alarm","alarmTime":"RFC3339","label":"..."}` |
| `clocks/commands/{id}/display_message` | Server→Device | `{"deviceId":"...","type":"display_message","message":"...","durationSeconds":N}` |
| `clocks/commands/{id}/set_brightness` | Server→Device | `{"deviceId":"...","type":"set_brightness","level":N}` |

### Published Topics

| Topic | QoS | Retain | Payload |
|---|---|---|---|
| `clocks/state/{id}/presence` | 1 | true | `"online"` / LWT `"offline"` |
| `clocks/state/{id}/alarm` | 1 | true | `{"time":"...","label":"...","active":bool}` |
| `clocks/state/{id}/display` | 1 | true | `{"mode":"clock","brightness":50}` |
| `clocks/events/{id}/heartbeat` | 0 | false | `{"uptime":N,"rssi":N,"ntp":bool,"heap":N}` |
| `clocks/events/{id}/command_result` | 1 | false | `{"type":"set_alarm","result":"applied"/"rejected","reason":"..."}` |
| `clocks/events/{id}/alarm_triggered` | 1 | false | `{"time":"...","label":"..."}` |
| `clocks/events/{id}/alarm_acknowledged` | 1 | false | `{"action":"dismiss"/"snooze","snoozeMinutes":5}` |

## Product Scope

In scope for the first version:

- PlatformIO project for ESP32 with `espressif32` platform, Arduino framework
- Dual-core FreeRTOS: NetworkTask on Core 0, DisplayTask on Core 1
- ILI9341 display driver via TFT_eSPI with hardware pin configuration
- XPT2046 touch input
- PubSubClient for MQTT with WiFi auto-reconnect
- ArduinoJson for JSON payload parsing
- NTP time sync
- RTC/NVS for alarm persistence across reboots
- Inter-task queue communication (no shared state)
- PWM backlight control
- Audio buzzer for alarms (GPIO26 DAC)
- RGB LED for status indication
- `credentials.h.example` pattern (compile-time defines, no runtime secrets)

Out of scope for the first version:

- Web-based configuration portal
- OTA firmware updates
- SD card support (hardware exists, not needed for v1)
- BLE or other connectivity besides WiFi
- Advanced alarm features (recurring, smart dismiss)
- MQTT TLS (plain TCP with home-network MQTT broker)

## Acceptance Criteria

- Flashed ESP32 boots, connects to WiFi, connects to MQTT broker
- Publishes `online` presence on connect, `offline` via LWT on disconnect
- Subscribes to `clocks/commands/{id}/#` and processes all three command types
- Displays clock face with correct time from NTP
- Responds to `set_brightness` with PWM backlight change (persisted)
- Responds to `display_message` — shows overlay, auto-dismisses after duration
- Responds to `set_alarm` — stores alarm, rings at alarm time, dismissible via touch
- Publishes `command_result` for each command received
- Publishes `heartbeat` every 30-60 seconds
- Recovers WiFi and MQTT automatically after network drop
- Unit tests pass for controller logic on native host

## Key Decisions

- **Dual-core FreeRTOS** — NetworkTask (Core 0) + DisplayTask (Core 1) with queue isolation
- **Interface-based architecture** — IMqttClient, IDisplay, ITouch, IAlarm interfaces for testability
- **Compile-time configuration** — WiFi/MQTT credentials via platformio build_flags / credentials.h
- **MQTT topic format** matches clock-server and mqtt-mcp-server contract
- **ArduinoJson 7** for JSON parsing (matching existing deps)
- **TFT_eSPI** for display (widely used, well-maintained ILI9341 driver)
- **PubSubClient** for MQTT (same as MqttDht22, proven pattern)

## Open Questions

- Default device ID? (`clock-1` from firmware, configurable)
- Default brightness level? (50% suggested)
- Snooze duration? (5 minutes default suggested)

## Next Step

Use the Architect Agent to produce `architecture.md` and decompose into epics and stories in `epics.md`.
