---
stepsCompleted: [1, 2]
inputDocuments:
  - project-docs/planning-artifacts/prds/prd-mqtt-smart-clock-2026-06-06/prd.md
  - project-docs/planning-artifacts/architecture.md
---

# mqtt-smart-clock — Epic Breakdown

## Overview

This document provides the complete epic and story breakdown for mqtt-smart-clock firmware, decomposing requirements from the PRD and Architecture into implementable stories.

## Requirements Inventory

FR1: WiFi connect and auto-reconnect with compile-time credentials.
FR2: MQTT connect, subscribe to `{prefix}/commands/{deviceId}/#`, LWT, auto-reconnect.
FR3: NTP time sync on boot.
FR4: ILI9341 clock face with HH:MM, date, day of week.
FR5: Priority-ordered display modes (alarm > message > clock > error).
FR6: set_alarm command — parse, validate, persist, publish command_result.
FR7: display_message command — parse, show overlay, auto-dismiss.
FR8: set_brightness command — parse, set PWM level, persist.
FR9: Trigger alarm — buzzer, LED pulse, alarm_triggered event.
FR10: Dismiss/snooze via touch — alarm_acknowledged event.
FR11: Heartbeat — periodic event with uptime, RSSI, NTP, heap.
FR12: Presence — online (retained) on boot, offline via LWT.

## Epic List

### Epic 1: Network Foundation And Clock Face

The device boots, connects to WiFi and MQTT, syncs time from NTP, and displays a functional clock face. Network recovery works without manual intervention.

**FRs covered:** FR1, FR2, FR3, FR4, FR12

### Epic 2: Command Processing And Display Modes

The device receives and processes all three MQTT commands — set_alarm, display_message, set_brightness — with proper display state management and MQTT event reporting.

**FRs covered:** FR5, FR6, FR7, FR8, FR11

### Epic 3: Alarm Ringing And Touch Interaction

The device triggers alarms at the scheduled time, plays audio, pulses LEDs, and responds to touch dismiss/snooze. Full alarm lifecycle is complete.

**FRs covered:** FR9, FR10

## Epic 1: Network Foundation And Clock Face

The device boots, connects to WiFi and MQTT, syncs time from NTP, and displays a functional clock face. Network recovery works without manual intervention.

### Story 1.1: Project Scaffolding And Build System

As a developer,
I want the PlatformIO project scaffolded with build configuration, hardware pin definitions, and interface headers,
So that subsequent stories build on a consistent structure.

**Acceptance Criteria:**

**Given** the repository is ready for firmware implementation
**When** the project is scaffolded
**Then** `platformio.ini` defines the `hosyond_28_esp32` environment with `espressif32` platform, `esp32dev` board, Arduino framework, and all library dependencies (TFT_eSPI, XPT2046_Touchscreen, PubSubClient, ArduinoJson)
**And** `include/board_config.h` defines all pin mappings from the Hosyond 2.8" display schematic
**And** `include/device_config.h` defines device ID, NTP server, timezone offset, MQTT topic prefix with `#ifndef` guards
**And** `include/credentials.h.example` documents all required WiFi/MQTT credentials
**And** `include/mqtt_topics.h` provides topic string builder functions
**And** `include/queue_types.h` defines `CommandMessage` and `EventMessage` structs
**And** `include/queue_handles.h` declares FreeRTOS queue handle externs
**And** `.gitignore` excludes `credentials.h`, `.pio/`, `.vscode/`
**And** The project compiles with `pio run`

### Story 1.2: NetworkTask With WiFi, MQTT, And NTP

As a user,
I want the device to connect to WiFi and MQTT automatically on boot and stay connected,
So that it can receive commands without manual setup.

**Acceptance Criteria:**

**Given** valid WiFi and MQTT credentials are configured
**When** the device boots
**Then** it connects to WiFi within 15 seconds, connects to MQTT broker, subscribes to `clocks/commands/{deviceId}/#`, publishes `online` (retained) to `clocks/state/{deviceId}/presence`, and syncs time from NTP
**And** the NetworkTask runs on Core 0 with stack size 8192 and priority 1

**Given** WiFi drops after initial connection
**When** the network is lost
**Then** NetworkTask retries WiFi every 5 seconds until reconnected, then reconnects MQTT and resubscribes

**Given** MQTT broker drops after initial connection
**When** the connection is lost
**Then** NetworkTask retries MQTT every 5 seconds until reconnected, resubscribes to command topics
**And** LWT publishes `offline` to `clocks/state/{deviceId}/presence`

**Given** the device boots for the first time or after a network outage
**When** NTP sync fails
**Then** it retries NTP sync every 30 seconds until successful

**Given** the device is running
**When** it receives a PubSubClient `callback`
**Then** it parses the topic to extract commandType, parses the JSON payload, validates deviceId, creates a `CommandMessage`, and pushes it to the commandQueue (non-blocking)
**And** invalid JSON or mismatched deviceId results in a logged warning — no crash

**Given** the eventQueue contains an `EventMessage`
**When** NetworkTask polls the queue
**Then** it serializes the event data to JSON and publishes to the specified MQTT topic with correct QoS and retain flags

### Story 1.3: Clock Face Display

As a user,
I want to see the current time on a readable clock face,
So that the device functions as a desk clock.

**Acceptance Criteria:**

**Given** NTP has synced time and the RTC is running
**When** DisplayTask renders the clock face
**Then** it displays HH:MM in large digits centered on the screen, SS in smaller digits, and the date + day of week below
**And** the display updates every second from the RTC
**And** time format (12h/24h) is configurable via `device_config.h`

**Given** the display hardware is initialized
**When** DisplayTask runs
**Then** TFT_eSPI is initialized with the correct pin configuration from `board_config.h`
**And** backlight is set to the default level (50%) on first boot, or the persisted level on subsequent boots
**And** DisplayTask runs on Core 1 with stack size 8192 and priority 5

**Given** the device has been running for some time
**When** the backlight level is changed via `set_brightness`
**Then** the level is persisted in NVS and restored on next boot

**Given** the device is in clock face mode
**When** no commands or alarms are active
**Then** it remains in clock face mode (lowest priority, default state)

## Epic 2: Command Processing And Display Modes

The device receives and processes all three MQTT commands — set_alarm, display_message, set_brightness — with proper display state management and MQTT event reporting.

### Story 2.1: Display State Machine And Message Overlay

As a user,
I want messages displayed on the clock when received,
So that I can see reminders and notifications.

**Acceptance Criteria:**

**Given** a `display_message` command arrives on the commandQueue
**When** DisplayTask processes it
**Then** the display switches to message overlay mode showing the message text centered on screen
**And** a countdown of remaining duration is shown
**And** the overlay auto-dismisses after `durationSeconds`, returning to the clock face
**And** DisplayTask pushes a `CommandResult` event to the eventQueue with result `applied`

**Given** a `display_message` command arrives while a message is already showing
**When** the new message is processed
**Then** the display updates to the new message and resets the duration timer

**Given** the display state machine exists
**When** any command is received
**Then** the display mode transitions according to priority: alarm_ringing > message > clock > error
**And** returning to a lower-priority mode restores the previous content seamlessly

### Story 2.2: Set Brightness Command

As a user,
I want to adjust the display brightness remotely,
So that it's comfortable day and night.

**Acceptance Criteria:**

**Given** a `set_brightness` command arrives on the commandQueue
**When** DisplayTask processes it
**Then** it clamps the level to 0-100, scales to PWM duty cycle, adjusts backlight GPIO21 via `ledc` or `analogWrite`
**And** persists the level in NVS
**And** pushes a `CommandResult` event with result `applied`
**And** pushes a `DisplayState` event with the new brightness level

**Given** the level is 0
**When** the command is processed
**Then** the backlight turns off (display still functions, just not visible)

### Story 2.3: Set Alarm Command

As a user,
I want to set an alarm on the clock remotely,
So that I can be woken or reminded at a specific time.

**Acceptance Criteria:**

**Given** a `set_alarm` command arrives on the commandQueue
**When** DisplayTask processes it
**Then** it parses the RFC3339 alarm time, converts to local time using configured timezone offset
**And** stores the alarm in NVS for persistence across reboots
**And** pushes a `CommandResult` event with result `applied`
**And** pushes an `AlarmState` event with the current alarm schedule

**Given** the alarm time is in the past
**When** DisplayTask processes it
**Then** it rejects the command and pushes a `CommandResult` event with result `rejected` and reason "alarm time is in the past"

**Given** a new `set_alarm` command arrives when an alarm is already set
**When** DisplayTask processes it
**Then** it replaces the existing alarm with the new one

**Given** no alarm has been configured
**When** the alarm evaluation check runs
**Then** no action is taken

### Story 2.4: Heartbeat And Event Publishing

As an operator,
I want the device to report its health and state,
So that I can monitor it remotely.

**Acceptance Criteria:**

**Given** the device is running
**When** DisplayTask's periodic tick fires (every 30 seconds)
**Then** it pushes a Heartbeat event to the eventQueue containing uptime, WiFi RSSI, NTP sync status, and free heap bytes
**And** NetworkTask publishes this to `clocks/events/{deviceId}/heartbeat` at QoS 0, no retain

**Given** the device boots and MQTT connects
**When** NetworkTask completes the connection
**Then** it publishes `online` to `clocks/state/{deviceId}/presence` with retained=true

**Given** the device disconnects unexpectedly
**When** MQTT connection drops
**Then** LWT publishes `offline` to `clocks/state/{deviceId}/presence` with retained=true

## Epic 3: Alarm Ringing And Touch Interaction

The device triggers alarms at the scheduled time, plays audio, pulses LEDs, and responds to touch dismiss/snooze.

### Story 3.1: Alarm Evaluation And Triggering

As a user,
I want the alarm to ring at the correct time,
So that I am notified.

**Acceptance Criteria:**

**Given** an alarm is stored in NVS
**When** the RTC time matches the alarm time
**Then** DisplayTask switches to alarm_ringing display mode (highest priority)
**And** activates the audio buzzer (GPIO26 DAC with GPIO4 enable)
**And** pulses RGB LED (GPIO22/16/17) in an alarm pattern
**And** pushes an `AlarmTriggered` event with timestamp and label
**And** the alarm continues ringing for up to 60 seconds or until dismissed

**Given** the alarm has been ringing for 60 seconds
**When** no dismiss action occurs
**Then** the alarm auto-stops, returns to clock face, and publishes an `AlarmAcknowledged` event with action "timeout"

### Story 3.2: Touch Dismiss And Snooze

As a user,
I want to silence the alarm by touching the screen,
So that it stops ringing when I'm awake.

**Acceptance Criteria:**

**Given** the display is in alarm_ringing mode
**When** the user touches the "Dismiss" button on screen
**Then** the alarm stops, display returns to clock face, RGB LED returns to idle
**And** an `AlarmAcknowledged` event with action "dismiss" is pushed to the eventQueue

**Given** the display is in alarm_ringing mode
**When** the user touches the "Snooze (5 min)" button on screen
**Then** the alarm stops, is re-armed for 5 minutes from now, display returns to clock face
**And** an `AlarmAcknowledged` event with action "snooze" and snoozeMinutes=5 is pushed to the eventQueue

**Given** the XPT2046 touch controller is initialized
**When** a touch occurs on the display
**Then** the coordinates are mapped to button regions
**And** touch events are debounced with a 200ms minimum interval to prevent double-triggers
