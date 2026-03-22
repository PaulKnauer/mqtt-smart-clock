---
stepsCompleted: ['step-01-init', 'step-02-discovery', 'step-02b-vision', 'step-02c-executive-summary', 'step-03-success', 'step-04-journeys', 'step-05-domain', 'step-06-innovation', 'step-07-project-type', 'step-08-scoping', 'step-09-functional', 'step-10-nonfunctional', 'step-11-polish', 'step-12-complete']
completedAt: '2026-03-22'
inputDocuments: ['docs/lcd-reference.md']
workflowType: 'prd'
classification:
  projectType: 'IoT system - embedded firmware + backend service extension'
  domain: 'IoT / Smart Home'
  complexity: 'medium-high'
  projectContext: 'brownfield'
briefCount: 0
researchCount: 0
brainstormingCount: 0
projectDocsCount: 1
---

# Product Requirements Document - mqtt-smart-clock

**Author:** Paul
**Date:** 2026-03-22

## Executive Summary

The mqtt-smart-clock project delivers a distributed smart clock system built on MQTT — designed for personal use by home automation enthusiasts who want a glanceable, ambient home information display they can extend and evolve over time. Existing smart clocks are closed, single-integration devices. This project treats the clock as a dumb display endpoint driven by an open MQTT message bus, enabling any home event — calendar reminders, sensor triggers, automation rules, custom notifications — to be surfaced on any clock in the home without firmware changes.

The system comprises two components: embedded TFT clock device firmware (Hosyond ESP32 2.8" TFT board) and an extension to the existing `clock-server` backend to ingest device-originated events. Time is maintained on-device via NTP, decoupling clock accuracy from backend availability.

**Target users:** Primarily the builder (Paul), with a secondary audience of home automation hobbyists comfortable with MQTT and embedded tinkering. The system is designed as an evolving platform, not a finished product.

**Project context:** Brownfield — integrates with existing `clock-server` MQTT command infrastructure (IoT / Smart Home, medium-high complexity).

### What Makes This Special

The core insight is decoupling *what to display* from *how to display it*. MQTT acts as the integration layer — any upstream system (Home Assistant, calendar services, custom scripts) publishes to a well-defined topic structure, and any clock in the home renders it. Adding a new notification type requires no firmware update: only a new MQTT publisher. The distributed architecture means multiple clocks serve different rooms without duplicating logic. The design is tinkerer-first: opinionated enough to work out of the box, open enough to grow indefinitely.

## Success Criteria

### User Success

- Clock displays correct local time and date at all times after NTP sync completes on boot
- Alarm set via `clock-server` HTTP API fires on the device at the correct time with an audible/visible indicator; user can dismiss via touch
- A `display_message` command appears on the correct clock within 2 seconds of publish and returns to clock view after `durationSeconds` without user interaction
- Brightness adjusts immediately on `set_brightness` and persists across power cycles
- Device recovers from Wi-Fi or MQTT disconnection automatically without requiring a reboot
- A second clock instance can be provisioned with configuration changes only — no code changes required

### Business Success

- Personal/hobbyist project; success is measured by daily use, not revenue
- **3-month:** Single clock running reliably, handling alarms and messages without manual intervention
- **6-month:** Two clock instances in different rooms; at least one external integration (e.g., Home Assistant, calendar) publishing notifications via MQTT
- **Ongoing:** New capabilities are added by writing new MQTT publishers, not by reflashing firmware

### Technical Success

- Device maintains accurate time (±1 second) via NTP after initial sync; re-syncs after reconnect
- MQTT reconnection within 30 seconds of connection loss; subscriptions restored automatically
- Commands with invalid `deviceId` are rejected with a `command_result` event (`status: rejected`), not silently ignored
- Device publishes presence (`online`/`offline` via LWT) and heartbeat every 30–60 seconds
- Alarm triggers locally even when MQTT broker is unreachable, provided NTP time is valid
- Firmware builds reproducibly via PlatformIO with documented configuration

### Measurable Outcomes

| Outcome | Target |
|---|---|
| Time accuracy after NTP sync | ±1 second |
| MQTT reconnect time | ≤ 30 seconds |
| Message display latency | ≤ 2 seconds from publish |
| Alarm trigger accuracy | ≤ 1 second of scheduled time |
| Brightness persistence | Survives power cycle |
| Multi-device support | ≥ 2 independent instances via config only |

## Product Scope

### MVP — Phase 1

Core device firmware and MQTT integration for daily use as a home clock with remote alarm and messaging:

- Always-on TFT clock face: time (HH:MM:SS), date, local timezone via NTP
- NTP sync on boot; retry on failure; re-sync after reconnect
- MQTT over TLS: connect, subscribe to `clocks/commands/{device_id}/#`, auto-reconnect + resubscribe
- `set_alarm`: store alarm, display next alarm on screen, trigger at scheduled time locally
- `display_message`: overlay message on TFT for `durationSeconds`, return to clock
- `set_brightness`: adjust TFT backlight immediately, persist in NVS
- `deviceId` validation on all commands; reject non-matching with `command_result`
- MQTT event publishing: presence (online/LWT offline), heartbeat (30–60s), `alarm_triggered`, `alarm_acknowledged`, `command_result`, alarm state snapshot, display state snapshot
- Config-only device identity (device_id, Wi-Fi, MQTT credentials excluded from source)
- Watchdog timer
- `clock-server`: ingestion of all device-originated events (presence, heartbeat, alarm state, display state, alarm_triggered, alarm_acknowledged, command_result)

### Growth — Phase 2

- Touch-to-snooze on alarm ringing screen
- Alarm persistence across reboots (NVS)
- Connectivity health indicator (RGB LED + on-screen icon)
- Backend HTTP APIs for reading device state (presence, display state, alarm state)
- Multi-alarm queue
- Local settings mode (on-screen Wi-Fi/config without reflash)
- OTA firmware updates

### Vision — Phase 3

- Display zones / layout regions (persistent info strips alongside clock)
- Home Assistant MQTT discovery integration
- Room-aware notification routing
- Multi-clock orchestration (alarm rings on all clocks)
- Sensor data publishing (ambient light, temperature if hardware supports)
- Web-based configuration UI

### Scoping Strategy & Risk

**MVP approach:** Platform MVP — validate that the open MQTT display bus architecture is reliable and useful as a daily-use home clock. Solo developer (Paul); embedded C++/Arduino experience; PlatformIO toolchain.

**Technical risks:**
- *Non-blocking event loop:* Use FreeRTOS tasks; assign display refresh to one task, MQTT/network to another — prototype multi-task architecture before full integration
- *TLS memory overhead:* Test TLS connection early; if memory-constrained, evaluate mbedTLS config trimming
- *Touch driver compatibility:* Confirm touch IC model (XPT2046 or CST816) from Hosyond schematic before starting display work

**Resource risk:** If time-constrained, drop `clock-server` event ingestion from MVP and focus on device firmware first — the backend extension is additive.

## User Journeys

### Journey 1: Paul Sets a Morning Alarm (Primary — Happy Path)

Paul POSTs `set_alarm` for `clock-bedroom` at `2026-03-23T06:30:00Z`, label "Wake up" to `clock-server`'s HTTP API. The server publishes the MQTT command. The bedroom clock stores the alarm, publishes an alarm state snapshot, and shows "Wake up — 06:30" on screen. Paul goes to sleep.

At 6:30am, the clock evaluates the alarm locally. The speaker sounds, RGB LED pulses, display shifts to `alarm_ringing`. Paul taps to dismiss. The clock publishes `alarm_acknowledged` with `action: dismiss` and returns to the clock face.

### Journey 2: Calendar Notification Mid-Morning (Primary — Integration Path)

A Home Assistant automation publishes `display_message` to `clocks/commands/clock-kitchen/display_message` with message "Standup in 5 min" and `durationSeconds: 60`. The kitchen clock shows the message for 60 seconds then returns to clock mode. Paul, making coffee, catches the reminder without touching his phone.

### Journey 3: Provisioning a Second Clock (Tinkerer/Integrator)

Paul flashes the same firmware onto a second board, sets `device_id` to `clock-bedroom` and Wi-Fi credentials in config, and powers it on. The device connects, syncs NTP, subscribes to `clocks/commands/clock-bedroom/#`, and publishes `online`. No server code changes needed. A test `display_message` confirms it works.

### Journey 4: Troubleshooting a Missed Message (Ops/Diagnostics)

A `display_message` doesn't appear on the kitchen clock. Paul checks `clocks/events/clock-kitchen/command_result` — the device published `status: rejected`, noting the `deviceId` mismatch (`clock_kitchen` vs `clock-kitchen`). He fixes the automation and republishes. The `command_result` event made the problem self-diagnosing.

### Journey Requirements Summary

| Capability | Required By |
|---|---|
| Alarm scheduling + local evaluation | Journey 1 |
| Alarm dismiss UX + `alarm_acknowledged` event | Journey 1 |
| Alarm state display on screen | Journey 1 |
| `display_message` overlay with auto-dismiss | Journey 2 |
| Display priority (alarm > message > clock) | Journey 2 |
| Config-only multi-device instantiation | Journey 3 |
| Presence publishing on connect | Journey 3 |
| `command_result` with `deviceId` validation | Journey 4 |
| MQTT-observable device state (all state topics) | Journeys 3, 4 |

## Domain-Specific Requirements

### Connectivity & Resilience

- Device handles Wi-Fi disconnection, MQTT broker restarts, and router reboots without manual reboot
- NTP sync failure at boot: display time-unavailable indicator, retry on interval; alarm evaluation blocked until first successful NTP sync
- After NTP is synced and connectivity is lost, device continues displaying time via internal RTC; alarms may still trigger locally

### Security

- MQTT transport uses TLS (`mqtts://`) in any non-isolated network; plain `mqtt://` acceptable only on fully isolated local LAN
- MQTT broker requires credential authentication; no unauthenticated broker access
- Device credentials (Wi-Fi SSID/password, MQTT credentials) stored in config excluded from version control; not hardcoded in firmware source
- No PII transmitted; security posture targets home network topology confidentiality

### Integration Patterns

- MQTT topic structure (`clocks/commands/`, `clocks/events/`, `clocks/state/`) is a versioned integration contract — breaking changes require coordination with all publishers
- QoS levels and retained flags per topic type are load-bearing (defined in `docs/lcd-reference.md`); deviations require explicit justification
- Third-party publishers must conform to defined JSON payload schemas; the device validates `deviceId` and rejects non-conforming payloads with `command_result`

## Innovation & Novel Patterns

### Detected Innovation Areas

**1. Display Abstraction Layer** — The firmware is a generic MQTT-driven display endpoint; the clock face is the default persona. The same firmware and command contract can drive any ambient display use case (kitchen timer, build status board, departure board) by changing only what upstream publishers send.

**2. Server-Optional Architecture** — `clock-server` adds HTTP convenience and scheduling, but the core display contract works with any MQTT publisher directly. The MQTT topic contract is the product; `clock-server` is an optional convenience layer.

**3. Unix Philosophy Applied to IoT** — MQTT topics are pipes; each device does one thing well; publishers and subscribers are composable. Adding a capability means writing a new publisher, not modifying the device or server.

**4. Bidirectional Sensor Node** — Each clock publishes `wifiRssi`, `uptimeSeconds`, `ntpSynced`, and `mqttConnected` in heartbeats, making each device an observable home health node alongside its display role.

**5. Layout Region Model (Future)** — The full-screen message model can evolve to named display zones (persistent strips for calendar events, temperature, status) without breaking existing command types.

### Market Context

Existing smart clocks (Amazon Echo Wall Clock, Lenovo Smart Clock, Google Nest Hub) are closed systems: first-party integrations only, non-extensible firmware, vendor-cloud multi-device coordination. This project's moat is architectural openness: any MQTT publisher can drive any clock.

### Validation Approach

- **Server-optional:** Publish `display_message` directly from a Home Assistant script (bypassing `clock-server`); confirm the clock responds correctly
- **Unix composability:** Wire a new notification source (e.g., weather script) to `display_message` in under 30 minutes with zero firmware or server changes
- **Sensor node:** Consume heartbeat data in Grafana or Home Assistant; confirm device health is observable without custom tooling

### Innovation Risk Mitigation

- **Display abstraction scope creep:** Keep the protocol layer hardware-agnostic; hardware-specific rendering stays encapsulated in the display driver module
- **Server-optional complexity:** Document clearly which features require `clock-server` vs. any MQTT publisher; avoid coupling device to server-specific behaviors
- **Sensor node feature creep:** Heartbeat data is diagnostic in MVP; resist adding sensors until the core display contract is stable

## IoT Embedded — Technical Requirements

### Hardware

| Component | Specification |
|---|---|
| MCU | ESP32 (Hosyond 2.8" TFT board) |
| Display | 2.8" colour TFT with touch input |
| Alarm audio | Speaker via on-board speaker socket |
| Visual indicator | RGB addressable LED (NeoPixel or equivalent) |
| Input method | Capacitive/resistive touch on TFT |
| Power | USB-C (wall-powered; no battery backup requirement) |
| Build system | PlatformIO |

### Connectivity

- **Wi-Fi:** 802.11 b/g/n; WPA2
- **MQTT:** 3.1.1 over TLS (`mqtts://`); QoS 0 and 1 per `docs/lcd-reference.md` delivery rules table
- **NTP:** `configTime()` / `sntp`; server configurable (default: `pool.ntp.org`); sync on boot and after reconnect
- **Topics:** `clocks/commands/{device_id}/`, `clocks/events/{device_id}/`, `clocks/state/{device_id}/`

### Display & UI Architecture

- Distinct graphical screens per state: clock face, message overlay, alarm ringing, error, time-unavailable
- Display state machine priority: `alarm_ringing` > `message` > `clock` > `error`
- Touch input for alarm dismiss (MVP) and snooze (growth); no physical buttons required
- RGB LED: alarm ringing pulse, connectivity status indicator, next-alarm glow
- Speaker: alarm tone on trigger, silenced on touch dismiss
- Backlight always on; brightness configurable and persisted in NVS

### Security

- MQTT credentials (broker URL, username, password) in PlatformIO build env or config header excluded from version control
- TLS certificate validation enabled; plain `mqtt://` only for isolated local development
- No HTTP, Telnet, or debug interface exposed in production builds

### Firmware Update

- **MVP:** USB/PlatformIO flash only
- **Growth:** OTA via MQTT command or HTTP endpoint

### Library Candidates (PlatformIO)

| Concern | Candidate |
|---|---|
| Display | TFT_eSPI or LovyanGFX |
| MQTT | AsyncMqttClient or PubSubClient (evaluate TLS + reconnect) |
| NTP | ESP32 built-in `configTime()` / `sntp` |
| Touch | XPT2046 or CST816 driver (confirm from Hosyond schematic) |
| JSON | ArduinoJson |
| NVS | ESP32 Preferences library |

## Functional Requirements

### Time & Display

- FR1: The device displays the current local time, updated every second
- FR2: The device displays the current date
- FR3: The device syncs time via NTP on boot and after reconnect
- FR4: The device displays a time-unavailable indicator when NTP has not yet succeeded
- FR5: The device displays distinct visual screens for clock, message overlay, alarm ringing, and error states
- FR6: The device applies display priority: alarm ringing overrides message overlay, which overrides clock

### MQTT Command Handling

- FR7: The device subscribes to all command topics scoped to its configured device identity
- FR8: The device validates that the `deviceId` field in any received command matches its own configured identity before applying the command
- FR9: The device rejects commands with a mismatched or missing `deviceId` and publishes a `command_result` event with status `rejected`
- FR10: The device applies a `set_alarm` command by scheduling an alarm at the specified UTC time with the given label
- FR11: The device applies a `display_message` command by showing the message on screen for the specified duration, then returning to the previous state
- FR12: The device applies a `set_brightness` command by adjusting the display backlight level immediately
- FR13: The device persists the last applied brightness level so it survives power cycles
- FR14: The device publishes a `command_result` event after processing each received command, indicating whether the command was applied, rejected, or failed

### Alarm

- FR15: The device displays the next scheduled alarm time and label on the clock screen
- FR16: The device triggers an alarm at the scheduled local time without requiring a network connection at trigger time
- FR17: The device enters alarm ringing state on trigger: plays an audible tone, activates the RGB LED, and shows an alarm screen on the display
- FR18: The device allows the user to dismiss an active alarm via touch interaction on the display
- FR19: The device publishes an `alarm_triggered` event exactly once when an alarm starts ringing
- FR20: The device publishes an `alarm_acknowledged` event when the user dismisses an alarm, including the action type and source
- FR21: The device publishes an alarm state snapshot whenever the scheduled alarm changes

### Connectivity & Resilience

- FR22: The device connects to the configured Wi-Fi network on boot
- FR23: The device connects to the configured MQTT broker over TLS on boot
- FR24: The device automatically reconnects to Wi-Fi and MQTT after any disconnection without requiring a reboot
- FR25: The device resubscribes to all command topics after an MQTT reconnect
- FR26: The device publishes an `online` presence message on boot and after reconnect
- FR27: The device publishes an `offline` presence message via MQTT Last Will and Testament when disconnected

### Observability & Diagnostics

- FR28: The device publishes a heartbeat event on a regular interval containing connectivity and health status
- FR29: The device publishes a display state snapshot when display mode or brightness changes
- FR30: The device recovers automatically from firmware hangs via watchdog timer without requiring manual intervention

### Configuration & Identity

- FR31: The device uses a unique device identity configured at build time, used to scope all MQTT topic subscriptions and event publications
- FR32: The device loads Wi-Fi credentials, MQTT broker details, and device identity from configuration excluded from version control
- FR33: A new device instance can be provisioned by changing configuration values only, with no firmware code changes required

### Backend Event Ingestion (clock-server)

- FR34: `clock-server` subscribes to and ingests presence events from all connected devices
- FR35: `clock-server` subscribes to and ingests heartbeat events from all connected devices
- FR36: `clock-server` subscribes to and ingests alarm state snapshots from all connected devices
- FR37: `clock-server` subscribes to and ingests display state snapshots from all connected devices
- FR38: `clock-server` subscribes to and ingests `alarm_triggered` events from all connected devices
- FR39: `clock-server` subscribes to and ingests `alarm_acknowledged` events from all connected devices
- FR40: `clock-server` subscribes to and ingests `command_result` events from all connected devices

## Non-Functional Requirements

### Performance

- NFR1: The clock face updates every second with no visible lag or flicker
- NFR2: Touch input on the TFT registers and responds within 200ms
- NFR3: A `display_message` command appears on screen within 2 seconds of MQTT publish
- NFR4: Alarm triggers within ±1 second of the scheduled UTC time when NTP is synced
- NFR5: NTP sync and MQTT operations do not block display refresh; display loop runs independently of network I/O
- NFR6: Device boots to a functional clock display within 10 seconds under normal conditions (Wi-Fi available, NTP reachable)

### Security

- NFR7: All MQTT communication uses TLS 1.2 or higher; unencrypted `mqtt://` not permitted in production
- NFR8: Wi-Fi credentials, MQTT credentials, and device identity configuration are never committed to version control
- NFR9: The device exposes no HTTP, Telnet, or debug interface in production firmware builds
- NFR10: Firmware source contains no hardcoded secrets, broker hostnames, or network configuration

### Reliability

- NFR11: The device automatically reconnects to Wi-Fi and MQTT within 30 seconds of connection loss, without human intervention
- NFR12: The watchdog timer resets the device if firmware hangs for more than 10 seconds
- NFR13: The device evaluates and triggers a scheduled alarm using local NTP-synced time even if the MQTT broker is unreachable at alarm time
- NFR14: The device displays correct time via internal RTC for at least 1 hour after NTP sync is lost before showing a time-degraded indicator
- NFR15: Brightness settings persist across power cycles with no manual reconfiguration

### Integration

- NFR16: The device conforms to MQTT 3.1.1 and is compatible with standard-compliant brokers (Mosquitto, HiveMQ, EMQX)
- NFR17: All MQTT payloads are valid JSON; malformed payloads are rejected without crashing the device
- NFR18: The MQTT topic structure follows the versioned contract in `docs/lcd-reference.md`; topic name or payload schema changes are treated as breaking changes
- NFR19: NTP server is configurable (default: `pool.ntp.org`); not hardcoded
- NFR20: The firmware builds reproducibly from source using PlatformIO with no manual toolchain setup beyond documented prerequisites
