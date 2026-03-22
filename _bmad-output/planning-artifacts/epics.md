---
stepsCompleted: [1, 2, 3, 4]
inputDocuments:
  - '_bmad-output/planning-artifacts/prd.md'
  - '_bmad-output/planning-artifacts/architecture.md'
---

# mqtt-smart-clock - Epic Breakdown

## Overview

This document provides the complete epic and story breakdown for mqtt-smart-clock, decomposing the requirements from the PRD and Architecture into implementable stories.

## Requirements Inventory

### Functional Requirements

FR1: The device displays the current local time, updated every second
FR2: The device displays the current date
FR3: The device syncs time via NTP on boot and after reconnect
FR4: The device displays a time-unavailable indicator when NTP has not yet succeeded
FR5: The device displays distinct visual screens for clock, message overlay, alarm ringing, and error states
FR6: The device applies display priority: alarm ringing overrides message overlay, which overrides clock
FR7: The device subscribes to all command topics scoped to its configured device identity
FR8: The device validates that the `deviceId` field in any received command matches its own configured identity before applying the command
FR9: The device rejects commands with a mismatched or missing `deviceId` and publishes a `command_result` event with status `rejected`
FR10: The device applies a `set_alarm` command by scheduling an alarm at the specified UTC time with the given label
FR11: The device applies a `display_message` command by showing the message on screen for the specified duration, then returning to the previous state
FR12: The device applies a `set_brightness` command by adjusting the display backlight level immediately
FR13: The device persists the last applied brightness level so it survives power cycles
FR14: The device publishes a `command_result` event after processing each received command, indicating whether the command was applied, rejected, or failed
FR15: The device displays the next scheduled alarm time and label on the clock screen
FR16: The device triggers an alarm at the scheduled local time without requiring a network connection at trigger time
FR17: The device enters alarm ringing state on trigger: plays an audible tone, activates the RGB LED, and shows an alarm screen on the display
FR18: The device allows the user to dismiss an active alarm via touch interaction on the display
FR19: The device publishes an `alarm_triggered` event exactly once when an alarm starts ringing
FR20: The device publishes an `alarm_acknowledged` event when the user dismisses an alarm, including the action type and source
FR21: The device publishes an alarm state snapshot whenever the scheduled alarm changes
FR22: The device connects to the configured Wi-Fi network on boot
FR23: The device connects to the configured MQTT broker over TLS on boot
FR24: The device automatically reconnects to Wi-Fi and MQTT after any disconnection without requiring a reboot
FR25: The device resubscribes to all command topics after an MQTT reconnect
FR26: The device publishes an `online` presence message on boot and after reconnect
FR27: The device publishes an `offline` presence message via MQTT Last Will and Testament when disconnected
FR28: The device publishes a heartbeat event on a regular interval containing connectivity and health status
FR29: The device publishes a display state snapshot when display mode or brightness changes
FR30: The device recovers automatically from firmware hangs via watchdog timer without requiring manual intervention
FR31: The device uses a unique device identity configured at build time, used to scope all MQTT topic subscriptions and event publications
FR32: The device loads Wi-Fi credentials, MQTT broker details, and device identity from configuration excluded from version control
FR33: A new device instance can be provisioned by changing configuration values only, with no firmware code changes required
FR34: `clock-server` subscribes to and ingests presence events from all connected devices
FR35: `clock-server` subscribes to and ingests heartbeat events from all connected devices
FR36: `clock-server` subscribes to and ingests alarm state snapshots from all connected devices
FR37: `clock-server` subscribes to and ingests display state snapshots from all connected devices
FR38: `clock-server` subscribes to and ingests `alarm_triggered` events from all connected devices
FR39: `clock-server` subscribes to and ingests `alarm_acknowledged` events from all connected devices
FR40: `clock-server` subscribes to and ingests `command_result` events from all connected devices

### NonFunctional Requirements

NFR1: The clock face updates every second with no visible lag or flicker
NFR2: Touch input on the TFT registers and responds within 200ms
NFR3: A `display_message` command appears on screen within 2 seconds of MQTT publish
NFR4: Alarm triggers within ±1 second of the scheduled UTC time when NTP is synced
NFR5: NTP sync and MQTT operations do not block display refresh; display loop runs independently of network I/O
NFR6: Device boots to a functional clock display within 10 seconds under normal conditions
NFR7: All MQTT communication uses TLS 1.2 or higher; unencrypted `mqtt://` not permitted in production
NFR8: Wi-Fi credentials, MQTT credentials, and device identity configuration are never committed to version control
NFR9: The device exposes no HTTP, Telnet, or debug interface in production firmware builds
NFR10: Firmware source contains no hardcoded secrets, broker hostnames, or network configuration
NFR11: The device automatically reconnects to Wi-Fi and MQTT within 30 seconds of connection loss, without human intervention
NFR12: The watchdog timer resets the device if firmware hangs for more than 10 seconds
NFR13: The device evaluates and triggers a scheduled alarm using local NTP-synced time even if the MQTT broker is unreachable at alarm time
NFR14: The device displays correct time via internal RTC for at least 1 hour after NTP sync is lost before showing a time-degraded indicator
NFR15: Brightness settings persist across power cycles with no manual reconfiguration
NFR16: The device conforms to MQTT 3.1.1 and is compatible with standard-compliant brokers (Mosquitto, HiveMQ, EMQX)
NFR17: All MQTT payloads are valid JSON; malformed payloads are rejected without crashing the device
NFR18: The MQTT topic structure follows the versioned contract in `docs/lcd-reference.md`; topic name or payload schema changes are treated as breaking changes
NFR19: NTP server is configurable (default: `pool.ntp.org`); not hardcoded
NFR20: The firmware builds reproducibly from source using PlatformIO with no manual toolchain setup beyond documented prerequisites

### Additional Requirements

Architecture-derived technical requirements that affect implementation order and story scope:

- AR1: FreeRTOS queue types (`CommandMessage` and `EventMessage` structs) must be defined as a shared contract before either task is implemented — they are the inter-task API
- AR2: `include/credentials.h.example`, `include/device_config.h`, and `include/mqtt_topics.h` must be scaffolded before any developer can build the project (credentials.h is gitignored)
- AR3: `bblanchon/ArduinoJson @ ^7.0` must be added to `platformio.ini` lib_deps; v6 and v7 have incompatible APIs — pin before first implementation story
- AR4: A `[env:native]` test environment must be added to `platformio.ini` to enable host-based unit testing of `alarm_manager` and `mqtt_handler` without hardware
- AR5: `main.cpp` must contain no logic — only `xTaskCreatePinnedToCore()` calls for `DisplayTask` (Core 1, high priority) and `NetworkTask` (Core 0, normal priority)
- AR6: `NetworkTask` owns all MQTT client operations; `DisplayTask` owns all TFT and touch operations — no cross-task direct calls; all cross-task data transits FreeRTOS queues
- AR7: All MQTT topic strings are defined in `include/mqtt_topics.h` (firmware) and `clock-server/src/mqtt/topic_patterns.ts` (backend) — no inline topic strings anywhere else
- AR8: `DeviceEventSubscriber` in `clock-server/src/mqtt/subscriber.ts` is the sole location for MQTT subscriptions in the backend; all handler functions in `handlers/` are pure functions (receive payload, emit structured JSON log)
- AR9: Amplifier (GPIO4, active-low) must be muted (`HIGH`) before stopping tone via `ledcWriteTone(channel, 0)` to prevent audio pop

### UX Design Requirements

N/A — this project has no web or mobile UI. The device TFT display states (clock face, message overlay, alarm ringing, error, time-unavailable) are specified in the PRD display state machine (FR5, FR6) and architecture patterns. Touch interaction (FR18) is the sole user input channel.

### FR Coverage Map

| FR | Epic | Description |
|---|---|---|
| FR1 | Epic 1 | Time display, updated every second |
| FR2 | Epic 1 | Date display |
| FR3 | Epic 1 | NTP sync on boot and after reconnect |
| FR4 | Epic 1 | Time-unavailable indicator |
| FR5 | Epics 1–3 | Display states: clock/error/time-unavailable (E1), message (E2), alarm_ringing (E3) |
| FR6 | Epics 2–3 | Priority: message > clock (E2), alarm_ringing > message (E3) |
| FR7 | Epic 2 | Subscribe to command topics |
| FR8 | Epic 2 | deviceId validation |
| FR9 | Epic 2 | Reject mismatched deviceId with command_result |
| FR10 | Epic 3 | Apply set_alarm command |
| FR11 | Epic 2 | Apply display_message command |
| FR12 | Epic 2 | Apply set_brightness command |
| FR13 | Epic 2 | Persist brightness in NVS |
| FR14 | Epic 2 | Publish command_result for every command |
| FR15 | Epic 3 | Display next alarm on clock face |
| FR16 | Epic 3 | Local alarm trigger evaluation |
| FR17 | Epic 3 | Alarm ringing state (tone + LED + display) |
| FR18 | Epic 3 | Touch dismiss |
| FR19 | Epic 3 | Publish alarm_triggered event |
| FR20 | Epic 3 | Publish alarm_acknowledged event |
| FR21 | Epic 3 | Publish alarm state snapshot on change |
| FR22 | Epic 1 | Wi-Fi connect on boot |
| FR23 | Epic 1 | MQTT connect over TLS on boot |
| FR24 | Epic 1 | Auto-reconnect without reboot |
| FR25 | Epic 1 | Resubscribe after MQTT reconnect |
| FR26 | Epic 1 | Publish online presence on boot/reconnect |
| FR27 | Epic 1 | LWT offline presence |
| FR28 | Epic 1 | Heartbeat event on interval |
| FR29 | Epic 2 | Display state snapshot on mode/brightness change |
| FR30 | Epic 1 | Watchdog timer |
| FR31 | Epic 1 | Device identity scoped at build time |
| FR32 | Epic 1 | Credentials loaded from gitignored config |
| FR33 | Epic 1 | Multi-device via config only |
| FR34 | Epic 4 | clock-server ingests presence events |
| FR35 | Epic 4 | clock-server ingests heartbeat events |
| FR36 | Epic 4 | clock-server ingests alarm state snapshots |
| FR37 | Epic 4 | clock-server ingests display state snapshots |
| FR38 | Epic 4 | clock-server ingests alarm_triggered events |
| FR39 | Epic 4 | clock-server ingests alarm_acknowledged events |
| FR40 | Epic 4 | clock-server ingests command_result events |

## Epic List

### Epic 1: Connected Clock Foundation
Paul powers on the device and sees the correct local time and date on the TFT display, with the device connected to Wi-Fi and MQTT, publishing presence and heartbeat — ready to receive commands. This epic establishes all project scaffolding, the dual FreeRTOS task architecture, connectivity, NTP time sync, the base clock/error/time-unavailable display states, device identity, and observability fundamentals.
**FRs covered:** FR1, FR2, FR3, FR4, FR5 (clock/error/time-unavailable states), FR22, FR23, FR24, FR25, FR26, FR27, FR28, FR30, FR31, FR32, FR33
**ARs covered:** AR1, AR2, AR3, AR4, AR5, AR6, AR7, AR8, AR9

### Epic 2: MQTT Command Processing & Remote Messaging
Any MQTT publisher can display a message on the clock within 2 seconds, adjust brightness persistently, and receive diagnostic feedback via `command_result` — making the clock a real-time home notification surface. Adds full MQTT command handling pipeline, display_message overlay, set_brightness with NVS persistence, and display state snapshots.
**FRs covered:** FR5 (message overlay state), FR6 (message > clock priority), FR7, FR8, FR9, FR11, FR12, FR13, FR14, FR29

### Epic 3: Alarm System
Paul sets an alarm via MQTT, the clock displays the next alarm time, triggers locally at the exact scheduled time with an audible tone, RGB LED, and alarm screen, and he can dismiss it with a touch — all without requiring a network connection at trigger time. Completes the display state machine.
**FRs covered:** FR5 (alarm_ringing state), FR6 (alarm_ringing > message priority), FR10, FR15, FR16, FR17, FR18, FR19, FR20, FR21

### Epic 4: Backend Event Ingestion (clock-server)
Paul can observe the operational state of every clock from the `clock-server` side — all 7 event types (presence, heartbeat, alarm state, display state, alarm_triggered, alarm_acknowledged, command_result) are ingested and logged as structured JSON. Extends clock-server non-destructively; works for all device instances automatically.
**FRs covered:** FR34, FR35, FR36, FR37, FR38, FR39, FR40

## Epic 1: Connected Clock Foundation

Paul powers on the device and sees the correct local time and date on the TFT display, with the device connected to Wi-Fi and MQTT, publishing presence and heartbeat — ready to receive commands.

### Story 1.1: Project Scaffolding & Build System

As a developer,
I want all project configuration, shared contract types, and library dependencies in place,
So that the firmware can be built and flashed to the device with documented steps.

**Acceptance Criteria:**

**Given** the repo is freshly cloned,
**When** `cp include/credentials.h.example include/credentials.h` is run and credentials are filled in,
**Then** `pio run -e hosyond_28_esp32` completes without errors.

**Given** `platformio.ini` is reviewed,
**When** `lib_deps` is inspected,
**Then** it includes `bodmer/TFT_eSPI@^2.5.43`, `paulstoffregen/XPT2046_Touchscreen@^1.4`, `knolleary/PubSubClient@^2.8`, and `bblanchon/ArduinoJson@^7.0`.

**Given** `platformio.ini` is reviewed,
**When** the `[env:native]` section is inspected,
**Then** it exists with `platform = native` and `bblanchon/ArduinoJson@^7.0` as a lib_dep.

**Given** `include/credentials.h` exists,
**When** `.gitignore` is reviewed,
**Then** `include/credentials.h` and `clock-server/.env` are gitignored, and `include/credentials.h.example` is tracked.

**Given** `include/mqtt_topics.h` is reviewed,
**When** all topic constants are inspected,
**Then** it defines string constants for all command topics (set_alarm, display_message, set_brightness), event topics (presence, heartbeat, command_result, alarm_triggered, alarm_acknowledged), and state topics (alarm, display) using `DEVICE_ID` from `device_config.h` — no inline topic strings anywhere else.

**Given** `src/main.cpp` is reviewed,
**When** its contents are inspected,
**Then** it contains only `xTaskCreatePinnedToCore()` calls for `DisplayTask` (Core 1, high priority) and `NetworkTask` (Core 0, normal priority), and no other logic.

**Given** shared inter-task message types are defined,
**When** the `CommandMessage` and `EventMessage` struct definitions are reviewed,
**Then** both enums and all field types are defined, and `commandQueue` (capacity 8) and `eventQueue` (capacity 16) handle types are declared in a shared header.

### Story 1.2: Clock Face Display

As Paul,
I want the device TFT to show a clock face with time and date updating every second,
So that the device functions as a visible clock from the moment it powers on.

**Acceptance Criteria:**

**Given** the device is powered on,
**When** DisplayTask initializes the TFT,
**Then** a clock face renders showing time in HH:MM:SS format and the current date in 320×240 landscape orientation.

**Given** the clock face is displayed,
**When** 1 second elapses,
**Then** the displayed time updates with no visible flicker, tearing, or lag.

**Given** no NTP sync has yet occurred,
**When** the clock face state is active,
**Then** a time-unavailable indicator is shown in place of the time digits.

**Given** DisplayTask is running on Core 1,
**When** the display loop executes,
**Then** it never waits on any network I/O and continues rendering independently of NetworkTask state.

**Given** the display enters an unrecoverable error state,
**When** the error condition is detected,
**Then** an error indicator screen is rendered, distinct from the clock face and time-unavailable screens.

### Story 1.3: Wi-Fi & MQTT Connectivity

As Paul,
I want the device to connect to Wi-Fi and the MQTT broker over TLS on boot and automatically reconnect within 30 seconds if either drops,
So that the clock is always ready to receive commands without manual intervention.

**Acceptance Criteria:**

**Given** valid Wi-Fi credentials in `credentials.h`,
**When** the device boots,
**Then** NetworkTask connects to the configured Wi-Fi network within the 10-second boot window.

**Given** Wi-Fi is connected,
**When** the MQTT connection is established,
**Then** it uses TLS 1.2+ via `WiFiClientSecure`/mbedTLS with username/password authentication from `credentials.h`.

**Given** the MQTT connection is established,
**When** the LWT is registered,
**Then** the `offline` presence payload is set at the configured presence topic with `retain=true` and `QoS=1`.

**Given** the MQTT connection is established,
**When** subscriptions are set up,
**Then** the device subscribes to `clocks/commands/{deviceId}/#` at `QoS=1`.

**Given** Wi-Fi or MQTT connection drops,
**When** disconnection is detected,
**Then** NetworkTask reconnects both within 30 seconds using exponential backoff and resubscribes to all command topics after MQTT reconnect.

**Given** NetworkTask is executing its reconnect loop,
**When** reconnection is in progress,
**Then** DisplayTask continues rendering on Core 1 without any blocking or stutter.

**Given** a watchdog timer is configured,
**When** the firmware hangs for more than 10 seconds,
**Then** the device resets automatically without manual intervention.

**Given** production firmware is running,
**When** the device is connected,
**Then** no HTTP, Telnet, or debug server interface is exposed on any port.

### Story 1.4: NTP Time Sync

As Paul,
I want the device to sync accurate time from NTP after Wi-Fi connects so that the clock transitions from the time-unavailable indicator to displaying correct local time,
So that I can rely on the displayed time without manual adjustment.

**Acceptance Criteria:**

**Given** Wi-Fi is connected,
**When** NTP sync succeeds,
**Then** the displayed time shows correct local time accurate to ±1 second of the actual time.

**Given** NTP sync succeeds,
**When** the display updates,
**Then** the time-unavailable indicator is replaced by the clock face showing correct time.

**Given** Wi-Fi reconnects after a disconnection,
**When** NTP re-sync completes,
**Then** time accuracy is restored without requiring a device reboot.

**Given** NTP sync fails on boot,
**When** the device retries,
**Then** it retries on a configurable interval, the time-unavailable state is maintained, and the display loop is not blocked.

**Given** `device_config.h` defines `NTP_SERVER`,
**When** NTP is configured via `configTime()`,
**Then** the server address is read from the constant — not hardcoded in any source file.

**Given** NTP sync has previously succeeded and NTP connectivity is subsequently lost,
**When** the device continues running,
**Then** it displays correct time via internal RTC for at least 60 minutes before showing a time-degraded indicator (NFR14).

### Story 1.5: Presence & Heartbeat Publishing

As Paul,
I want to observe when each clock is online and its health status via MQTT,
So that I can detect connectivity issues across all clock instances without physically checking devices.

**Acceptance Criteria:**

**Given** MQTT connects successfully,
**When** connection is established,
**Then** the device publishes an `online` presence message to `clocks/events/{deviceId}/presence` with `retain=true` and `QoS=1`, containing `deviceId` and an RFC 3339 timestamp.

**Given** the device disconnects ungracefully,
**When** checked via MQTT subscription,
**Then** the LWT `offline` payload appears at the presence topic with `retain=true`.

**Given** the device is running,
**When** 30–60 seconds elapse,
**Then** the device publishes a heartbeat to `clocks/events/{deviceId}/heartbeat` containing `deviceId`, `wifiRssi`, `uptimeSeconds`, `ntpSynced`, `mqttConnected`, and an `at` RFC 3339 timestamp.

**Given** a heartbeat is published,
**When** the payload is inspected,
**Then** it is valid JSON with all required fields present and no malformed data.

**Given** the device reconnects after a disconnection,
**When** reconnect succeeds,
**Then** a new `online` presence message is published immediately.

## Epic 2: MQTT Command Processing & Remote Messaging

Any MQTT publisher can display a message on the clock within 2 seconds, adjust brightness persistently, and receive diagnostic feedback via `command_result` — making the clock a real-time home notification surface.

### Story 2.1: MQTT Command Infrastructure & deviceId Validation

As a developer,
I want the device to receive MQTT messages, parse JSON payloads, and validate the `deviceId` field before any command is processed,
So that all command handling is secured against mis-routed messages and parse errors surface diagnostically.

**Acceptance Criteria:**

**Given** an MQTT message arrives on a subscribed topic,
**When** `NetworkTask` receives it,
**Then** the raw payload is passed to `mqtt_handler` for JSON parsing using ArduinoJson v7.

**Given** a command payload has a `deviceId` that matches the device's configured identity,
**When** the command is parsed,
**Then** processing continues to the command-specific handler.

**Given** a command payload has a `deviceId` that does not match,
**When** the mismatch is detected,
**Then** the command is rejected and a `command_result` event is published with `status: "rejected"` and `detail: "device_id_mismatch"`, and the command is not applied.

**Given** a command payload is malformed JSON,
**When** the parse fails,
**Then** a `command_result` event is published with `status: "failed"` and `detail: "json_parse_error"`, and the device does not crash.

**Given** any command is received (valid or invalid),
**When** processing completes,
**Then** exactly one `command_result` event is published — no command is silently discarded.

### Story 2.2: Display Message Overlay

As a home automation user,
I want to publish a `display_message` command to a clock and have the message appear on screen within 2 seconds for the specified duration, then return to the clock face automatically,
So that I can surface time-sensitive notifications on any clock in the home without physical interaction.

**Acceptance Criteria:**

**Given** a valid `display_message` command is received,
**When** the command is applied,
**Then** the message text appears on the TFT display within 2 seconds of MQTT publish.

**Given** the message is displayed,
**When** `durationSeconds` elapses,
**Then** the display returns to the previous state (clock face) automatically with no user interaction.

**Given** a `display_message` command is active,
**When** the display state machine evaluates priority,
**Then** the message overlay state takes priority over the clock face state (`message > clock`).

**Given** a `display_message` command arrives,
**When** it is successfully applied,
**Then** a `command_result` event is published with `status: "applied"`.

**Given** a `display_message` is shown,
**When** the display state changes,
**Then** a display state snapshot is published to `clocks/state/{deviceId}/display` reflecting the new mode.

### Story 2.3: Brightness Control with Persistence

As Paul,
I want to send a `set_brightness` command to adjust the TFT backlight level immediately, with the setting persisting across power cycles,
So that I can configure each clock's brightness once without reconfiguring after reboots.

**Acceptance Criteria:**

**Given** a valid `set_brightness` command is received with a brightness value,
**When** the command is applied,
**Then** the TFT backlight level changes immediately to the specified level.

**Given** a brightness level is applied,
**When** the value is written to NVS via the `brightness` module,
**Then** the setting survives a power cycle and is restored on next boot without user action.

**Given** the device boots,
**When** `brightness::getBrightness()` is called during DisplayTask init,
**Then** the last persisted brightness level is applied (not a hardcoded default), unless no value has been previously stored.

**Given** a `set_brightness` command is applied,
**When** processing completes,
**Then** a `command_result` event is published with `status: "applied"` and a display state snapshot is published reflecting the new brightness.

**Given** `brightness.cpp` writes to NVS,
**When** its implementation is reviewed,
**Then** it is the only file in the codebase that calls `Preferences` NVS read/write operations.

## Epic 3: Alarm System

Paul sets an alarm via MQTT, the clock displays the next alarm time, triggers locally at the exact scheduled time with an audible tone, RGB LED, and alarm screen, and he can dismiss it with a touch — all without requiring a network connection at trigger time. Completes the display state machine.

### Story 3.1: Alarm Scheduling & Clock Face Display

As Paul,
I want to send a `set_alarm` command via MQTT and see the next scheduled alarm time and label displayed on the clock face,
So that I have visual confirmation the alarm is set before I rely on it.

**Acceptance Criteria:**

**Given** a valid `set_alarm` command is received with a UTC time and label,
**When** the command is applied by `alarm_manager`,
**Then** the alarm is stored in memory and a `command_result` event is published with `status: "applied"`.

**Given** an alarm is stored,
**When** the clock face renders,
**Then** the next scheduled alarm time and label are shown on the clock face display.

**Given** a `set_alarm` command is applied,
**When** the alarm state changes,
**Then** an alarm state snapshot is published to `clocks/state/{deviceId}/alarm` with `retain=true`, containing `deviceId`, alarm time, label, and armed status.

**Given** a `set_alarm` command has a `deviceId` that does not match,
**When** the mismatch is detected,
**Then** the command is rejected with `command_result status: "rejected"` and the stored alarm is unchanged.

### Story 3.2: Local Alarm Trigger & Ringing State

As Paul,
I want the clock to evaluate the scheduled alarm time every second against local NTP-synced time and enter alarm ringing state when the time is reached — even if MQTT is unreachable,
So that the alarm fires reliably as a local device function, not a cloud-dependent one.

**Acceptance Criteria:**

**Given** an alarm is armed and NTP is synced,
**When** the current local time reaches or passes the scheduled alarm time,
**Then** `alarm_manager` triggers the alarm and sets `alarmArmed = false` immediately to prevent re-trigger.

**Given** the alarm triggers,
**When** `DisplayTask` processes the trigger,
**Then** the display transitions to the `alarm_ringing` state, which takes priority over all other display states (`alarm_ringing > message > clock`).

**Given** the alarm triggers,
**When** the ringing state is active,
**Then** the LEDC PWM outputs an audible tone (e.g. 880 Hz) via GPIO26 with the amplifier enabled (GPIO4 LOW).

**Given** the alarm triggers,
**When** the ringing state is active,
**Then** the RGB LED activates in an alarm pattern (e.g. pulsing red).

**Given** the alarm triggers,
**When** `alarm_triggered` is evaluated,
**Then** the event is published exactly once to `clocks/events/{deviceId}/alarm_triggered` with `deviceId` and `at` RFC 3339 timestamp.

**Given** the MQTT broker is unreachable at alarm trigger time,
**When** the scheduled time is reached and NTP is synced,
**Then** the alarm triggers locally and enters ringing state regardless of broker connectivity.

**Given** NTP has not been synced,
**When** the scheduled alarm time would be reached,
**Then** the alarm does not trigger and remains armed until NTP sync is confirmed.

### Story 3.3: Alarm Dismiss via Touch

As Paul,
I want to dismiss the ringing alarm by tapping the TFT display,
So that I can silence the clock quickly without needing a phone or app.

**Acceptance Criteria:**

**Given** the alarm is in the ringing state,
**When** the user taps anywhere on the TFT display,
**Then** the touch input is registered within 200ms via XPT2046 on the HSPI bus.

**Given** a touch is registered during alarm ringing,
**When** dismiss is processed,
**Then** the alarm tone stops (amplifier muted via GPIO4 HIGH before `ledcWriteTone(channel, 0)` to prevent audio pop), the RGB LED deactivates, and the display returns to the clock face.

**Given** dismiss is processed,
**When** the `alarm_acknowledged` event is published,
**Then** it is sent to `clocks/events/{deviceId}/alarm_acknowledged` with `action: "dismiss"`, `source: "touch"`, `deviceId`, and `at` RFC 3339 timestamp.

**Given** dismiss is processed,
**When** an updated alarm state snapshot is published,
**Then** it reflects `alarmArmed: false` at `clocks/state/{deviceId}/alarm` with `retain=true`.

**Given** no touch occurs during alarm ringing,
**When** the alarm continues ringing,
**Then** the tone and LED remain active indefinitely until dismissed.

### Story 3.4: Host-Based Unit Tests for Alarm Logic

As a developer,
I want alarm evaluation logic and MQTT command parsing to be covered by host-based unit tests that run without hardware,
So that the core business logic can be verified without flashing a device.

**Acceptance Criteria:**

**Given** the `[env:native]` PlatformIO environment exists,
**When** `pio test -e native` is run,
**Then** all tests pass on the host without requiring an ESP32 device.

**Given** `alarm_manager` evaluation logic is tested,
**When** the test suite runs,
**Then** tests cover: alarm triggers when `ntpSynced=true` and `currentTime >= alarmTime` and `alarmArmed=true`; alarm does not trigger when `ntpSynced=false`; `alarmArmed` is set to `false` immediately after trigger.

**Given** `mqtt_handler` command parsing is tested,
**When** the test suite runs,
**Then** tests cover: valid JSON with matching `deviceId` accepted; `deviceId` mismatch rejected with correct status string; malformed JSON handled without crash; `command_result` status vocabulary matches exactly (`"received"`, `"applied"`, `"rejected"`, `"failed"`).

## Epic 4: Backend Event Ingestion (clock-server)

Paul can observe the operational state of every clock from the `clock-server` side — all 7 event types (presence, heartbeat, alarm state, display state, alarm_triggered, alarm_acknowledged, command_result) are ingested and logged as structured JSON. Extends clock-server non-destructively; works for all device instances automatically.

### Story 4.1: DeviceEventSubscriber Foundation

As Paul,
I want `clock-server` to subscribe to all device event topics using wildcard patterns and dispatch incoming payloads to typed handlers,
So that every clock instance is observable from the backend without any per-device configuration.

**Acceptance Criteria:**

**Given** `clock-server` starts,
**When** `DeviceEventSubscriber` initialises,
**Then** it subscribes to `clocks/+/events/#` and `clocks/+/state/#` wildcard topics using the MQTT client.

**Given** `DeviceEventSubscriber` is the subscription owner,
**When** the rest of the `clock-server` codebase is reviewed,
**Then** no other module creates MQTT subscriptions — all subscriptions are in `subscriber.ts`.

**Given** all topic string patterns are reviewed,
**When** `clock-server/src/mqtt/topic_patterns.ts` is inspected,
**Then** it defines all wildcard topic constants and no inline topic strings appear elsewhere in the backend codebase.

**Given** an MQTT message arrives on a subscribed topic,
**When** `DeviceEventSubscriber` receives it,
**Then** it extracts the event type from the topic structure and dispatches the raw payload to the correct typed handler.

**Given** an unknown event type arrives,
**When** `DeviceEventSubscriber` attempts dispatch,
**Then** it logs a structured JSON entry `{ topic, eventType, rawPayload }` and discards the message without throwing.

**Given** `clock-server` already has existing MQTT publisher code,
**When** `DeviceEventSubscriber` is added,
**Then** the existing publisher functionality is unchanged and no existing files are modified.

### Story 4.2: Presence & Heartbeat Ingestion

As Paul,
I want `clock-server` to ingest and log device presence and heartbeat events,
So that I can observe device connectivity and health across all clocks from the backend.

**Acceptance Criteria:**

**Given** a device publishes an `online` or `offline` presence message,
**When** the `presence` handler receives the payload,
**Then** it logs a structured JSON entry containing `deviceId`, `status`, `at` timestamp, and `topic`.

**Given** a device publishes a heartbeat event,
**When** the `heartbeat` handler receives the payload,
**Then** it logs a structured JSON entry containing `deviceId`, `wifiRssi`, `uptimeSeconds`, `ntpSynced`, `mqttConnected`, `at`, and `topic`.

**Given** either handler receives a malformed payload,
**When** JSON parsing fails,
**Then** a structured JSON error entry `{ topic, error, rawPayload }` is logged and no exception is thrown.

**Given** all handler functions are reviewed,
**When** their implementations are inspected,
**Then** each is a pure function: receives a parsed payload, emits a structured log entry, and has no other side effects.

### Story 4.3: Alarm & Display State Ingestion

As Paul,
I want `clock-server` to ingest alarm state and display state snapshots from all devices,
So that I can observe the current alarm schedule and display mode of any clock at any time.

**Acceptance Criteria:**

**Given** a device publishes an alarm state snapshot,
**When** the `alarmState` handler receives the payload,
**Then** it logs a structured JSON entry containing `deviceId`, `alarmTime`, `label`, `alarmArmed`, `at`, and `topic`.

**Given** a device publishes a display state snapshot,
**When** the `displayState` handler receives the payload,
**Then** it logs a structured JSON entry containing `deviceId`, `displayMode`, `brightness`, `at`, and `topic`.

**Given** either handler receives a malformed or incomplete payload,
**When** parsing or field extraction fails,
**Then** a structured JSON error entry `{ topic, error, rawPayload }` is logged and the handler returns without throwing.

### Story 4.4: Alarm Event & Command Result Ingestion

As Paul,
I want `clock-server` to ingest alarm lifecycle events and command results from all devices,
So that I can diagnose missed alarms, rejected commands, and confirm the MQTT contract is operating correctly end-to-end.

**Acceptance Criteria:**

**Given** a device publishes an `alarm_triggered` event,
**When** the `alarmTriggered` handler receives the payload,
**Then** it logs a structured JSON entry containing `deviceId`, `alarmTime`, `label`, `at`, and `topic`.

**Given** a device publishes an `alarm_acknowledged` event,
**When** the `alarmAcknowledged` handler receives the payload,
**Then** it logs a structured JSON entry containing `deviceId`, `action`, `source`, `at`, and `topic`.

**Given** a device publishes a `command_result` event,
**When** the `commandResult` handler receives the payload,
**Then** it logs a structured JSON entry containing `deviceId`, `command`, `status`, `detail` (if present), `at`, and `topic`.

**Given** a `command_result` status value is logged,
**When** the logged entry is reviewed,
**Then** the `status` field is one of exactly `"received"`, `"applied"`, `"rejected"`, or `"failed"` — matching the firmware vocabulary precisely.

**Given** any handler receives a malformed payload,
**When** parsing fails,
**Then** a structured JSON error entry `{ topic, error, rawPayload }` is logged and no exception propagates.
