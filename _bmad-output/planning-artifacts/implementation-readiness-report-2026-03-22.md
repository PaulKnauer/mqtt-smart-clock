---
stepsCompleted: [1, 2, 3, 4, 5, 6]
inputDocuments:
  - '_bmad-output/planning-artifacts/prd.md'
  - '_bmad-output/planning-artifacts/architecture.md'
  - '_bmad-output/planning-artifacts/epics.md'
---

# Implementation Readiness Assessment Report

**Date:** 2026-03-22
**Project:** mqtt-smart-clock

## PRD Analysis

### Functional Requirements (40 total)

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

### Non-Functional Requirements (20 total)

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
NFR16: The device conforms to MQTT 3.1.1 and is compatible with standard-compliant brokers
NFR17: All MQTT payloads are valid JSON; malformed payloads are rejected without crashing the device
NFR18: The MQTT topic structure follows the versioned contract in `docs/lcd-reference.md`
NFR19: NTP server is configurable (default: `pool.ntp.org`); not hardcoded
NFR20: The firmware builds reproducibly from source using PlatformIO with no manual toolchain setup beyond documented prerequisites

### PRD Completeness Assessment

PRD is complete and well-structured. All 40 FRs are numbered, unambiguous, and testable. All 20 NFRs include measurable targets (±1s, 30s, 200ms, 10s). No missing sections.

---

## Epic Coverage Validation

### Coverage Matrix

| FR | PRD Requirement (summary) | Epic/Story | Status |
|---|---|---|---|
| FR1 | Time display, every second | Epic 1 / Story 1.2 | ✅ Covered |
| FR2 | Date display | Epic 1 / Story 1.2 | ✅ Covered |
| FR3 | NTP sync on boot and reconnect | Epic 1 / Story 1.4 | ✅ Covered |
| FR4 | Time-unavailable indicator | Epic 1 / Stories 1.2, 1.4 | ✅ Covered |
| FR5 | Distinct screens for all states | Epics 1–3 / Stories 1.2, 2.2, 3.2 | ✅ Covered |
| FR6 | Display priority order | Epics 2–3 / Stories 2.2, 3.2 | ✅ Covered |
| FR7 | Subscribe to command topics | Epic 2 / Story 2.1 | ✅ Covered |
| FR8 | deviceId validation | Epic 2 / Story 2.1 | ✅ Covered |
| FR9 | Reject mismatched deviceId + command_result | Epic 2 / Story 2.1 | ✅ Covered |
| FR10 | Apply set_alarm command | Epic 3 / Story 3.1 | ✅ Covered |
| FR11 | Apply display_message command | Epic 2 / Story 2.2 | ✅ Covered |
| FR12 | Apply set_brightness command | Epic 2 / Story 2.3 | ✅ Covered |
| FR13 | Persist brightness in NVS | Epic 2 / Story 2.3 | ✅ Covered |
| FR14 | Publish command_result for every command | Epic 2 / Story 2.1 | ✅ Covered |
| FR15 | Display next alarm on clock face | Epic 3 / Story 3.1 | ✅ Covered |
| FR16 | Local alarm trigger evaluation | Epic 3 / Story 3.2 | ✅ Covered |
| FR17 | Alarm ringing state (tone + LED + display) | Epic 3 / Story 3.2 | ✅ Covered |
| FR18 | Touch dismiss | Epic 3 / Story 3.3 | ✅ Covered |
| FR19 | Publish alarm_triggered once | Epic 3 / Story 3.2 | ✅ Covered |
| FR20 | Publish alarm_acknowledged | Epic 3 / Story 3.3 | ✅ Covered |
| FR21 | Publish alarm state snapshot on change | Epic 3 / Story 3.1 | ✅ Covered |
| FR22 | Wi-Fi connect on boot | Epic 1 / Story 1.3 | ✅ Covered |
| FR23 | MQTT connect over TLS on boot | Epic 1 / Story 1.3 | ✅ Covered |
| FR24 | Auto-reconnect without reboot | Epic 1 / Story 1.3 | ✅ Covered |
| FR25 | Resubscribe after MQTT reconnect | Epic 1 / Story 1.3 | ✅ Covered |
| FR26 | Publish online presence | Epic 1 / Story 1.5 | ✅ Covered |
| FR27 | LWT offline presence | Epic 1 / Stories 1.3, 1.5 | ✅ Covered |
| FR28 | Heartbeat on interval | Epic 1 / Story 1.5 | ✅ Covered |
| FR29 | Display state snapshot on change | Epic 2 / Stories 2.2, 2.3 | ✅ Covered |
| FR30 | Watchdog timer | Epic 1 / Story 1.3 | ✅ Covered |
| FR31 | Device identity at build time | Epic 1 / Story 1.1 | ✅ Covered |
| FR32 | Credentials from gitignored config | Epic 1 / Story 1.1 | ✅ Covered |
| FR33 | Multi-device via config only | Epic 1 / Story 1.1 | ✅ Covered |
| FR34 | clock-server ingests presence | Epic 4 / Story 4.2 | ✅ Covered |
| FR35 | clock-server ingests heartbeat | Epic 4 / Story 4.2 | ✅ Covered |
| FR36 | clock-server ingests alarm state | Epic 4 / Story 4.3 | ✅ Covered |
| FR37 | clock-server ingests display state | Epic 4 / Story 4.3 | ✅ Covered |
| FR38 | clock-server ingests alarm_triggered | Epic 4 / Story 4.4 | ✅ Covered |
| FR39 | clock-server ingests alarm_acknowledged | Epic 4 / Story 4.4 | ✅ Covered |
| FR40 | clock-server ingests command_result | Epic 4 / Story 4.4 | ✅ Covered |

### Missing Requirements

None.

### Coverage Statistics

- Total PRD FRs: 40
- FRs covered in epics: 40
- Coverage: **100%**

---

## UX Alignment Assessment

### UX Document Status

Not found — deliberate decision made by Paul during this readiness session. The TFT display has 5 distinct screens and touch interaction, so UX is implied. Paul has explicitly chosen to proceed without a UX spec and return to add one later.

### Alignment Issues

None at this stage — the PRD and architecture fully describe the display state machine, priority rules, and data content for each screen. Visual layout, typography, and colour decisions are intentionally deferred.

### Warnings

⚠️ **Deferred UX** — Screen layout, typography, colours, and touch target sizing are not specified. The dev agent implementing Story 1.2 (clock face), Story 2.2 (message overlay), and Story 3.2 (alarm_ringing) will make visual decisions autonomously. Review visually after implementation and run `/bmad-create-ux-design` when ready to formalise the design.

---

## Epic Quality Review

### Epic Structure Validation

**Epic 1: Connected Clock Foundation** ✅
- Title is user-centric ("connected clock" is Paul's outcome). ✅
- Goal describes user outcome (sees correct time, device is ready). ✅
- Can stand alone — no dependency on Epics 2, 3, or 4. ✅

**Epic 2: MQTT Command Processing & Remote Messaging** ✅
- Title is user-centric (remote messaging is Paul's outcome). ✅
- Goal describes user outcome (messages appear within 2s). ✅
- Functions independently using only Epic 1 output (connectivity). ✅
- Does not require Epic 3 or 4. ✅

**Epic 3: Alarm System** ✅
- Title is user-centric (alarm is Paul's outcome). ✅
- Goal describes user outcome (set, trigger, dismiss alarm locally). ✅
- Uses Epic 1 output (connectivity); functions without Epic 2 or 4. ✅

**Epic 4: Backend Event Ingestion (clock-server)** ✅
- Title is user-centric (Paul observes device state). ✅
- Goal describes user outcome (all events visible from backend). ✅
- Can be implemented in parallel with any firmware epic post-Epic 1. ✅

### Story Quality Assessment

**🟡 Minor Concerns (3 instances — non-blocking):**

1. **Story 1.1, 2.1, 3.4 — "As a developer" user type**: Three stories use a developer persona rather than an end-user persona. This is acceptable for a brownfield embedded project (scaffolding and testing are genuine development deliverables), but deviates from pure user-story convention. No remediation required; note for awareness.

2. **Story 2.1 — Infrastructure story scope**: Story 2.1 establishes command parsing infrastructure used by Stories 2.2 and 2.3. It delivers diagnostic value (command_result) directly, which is user-observable. Acceptable as scoped.

**No 🔴 Critical or 🟠 Major violations found.**

### Dependency Analysis

**Within-epic story dependencies:**
- Epic 1: 1.1 → 1.2 → 1.3 → 1.4 → 1.5. Each depends only on previous stories. ✅
- Epic 2: 2.1 → 2.2 → 2.3. 2.2 and 2.3 each depend only on 2.1. ✅
- Epic 3: 3.1 → 3.2 → 3.3 → 3.4. Each depends only on previous stories. ✅
- Epic 4: 4.1 → 4.2 → 4.3 → 4.4. Each depends only on 4.1. ✅

**No forward dependencies found.**

### NFR Coverage Review

Cross-referencing all 20 NFRs against story acceptance criteria:

| NFR | Coverage | Status |
|---|---|---|
| NFR1 — no flicker | Story 1.2 AC | ✅ |
| NFR2 — touch 200ms | Story 3.3 AC | ✅ |
| NFR3 — message within 2s | Story 2.2 AC | ✅ |
| NFR4 — alarm ±1s | Story 3.2 (local evaluation, NTP synced) — no explicit AC for ±1s timing | 🟡 Minor gap |
| NFR5 — display not blocked | Stories 1.2, 1.3 ACs | ✅ |
| NFR6 — boot within 10s | Story 1.3 AC | ✅ |
| NFR7 — TLS 1.2+ | Story 1.3 AC | ✅ |
| NFR8 — no credentials in VCS | Story 1.1 AC | ✅ |
| NFR9 — no debug interface | Story 1.3 AC | ✅ |
| NFR10 — no hardcoded secrets | Story 1.1 AC (credentials.h/gitignore) | ✅ |
| NFR11 — reconnect ≤30s | Story 1.3 AC | ✅ |
| NFR12 — watchdog 10s | Story 1.3 AC | ✅ |
| NFR13 — alarm without network | Story 3.2 AC | ✅ |
| NFR14 — RTC for ≥1hr after NTP loss | **No story covers this** | 🟠 Gap |
| NFR15 — brightness persists | Story 2.3 AC | ✅ |
| NFR16 — MQTT 3.1.1 compatibility | Implied by Story 1.3 TLS/broker ACs — not explicitly tested | 🟡 Minor gap |
| NFR17 — valid JSON payloads | Stories 2.1, 4.1 ACs | ✅ |
| NFR18 — topic contract | Story 1.1 AC (mqtt_topics.h) | ✅ |
| NFR19 — NTP server configurable | Story 1.4 AC | ✅ |
| NFR20 — reproducible build | Story 1.1 AC | ✅ |

**NFR14 is the only meaningful gap.** The requirement states the device displays correct time for ≥1 hour after NTP sync is lost before showing a time-degraded indicator. No story addresses the distinction between "NTP never synced" (time-unavailable, Story 1.2/1.4) and "NTP synced then lost" (should display via RTC for ≥1hr, Story 1.4 only covers retry on failure).

---

## Summary and Recommendations

### Overall Readiness Status

**✅ READY FOR IMPLEMENTATION** — with one recommended story enhancement before Story 1.4 is developed.

### Issues Found

| # | Severity | Issue | Location |
|---|---|---|---|
| 1 | 🟠 Major | NFR14 not covered: device must maintain RTC time display for ≥1hr after NTP sync loss before showing degraded indicator | Story 1.4 missing AC |
| 2 | 🟡 Minor | NFR4 (±1s alarm accuracy) not explicitly tested in any AC | Story 3.2 |
| 3 | 🟡 Minor | NFR16 (MQTT 3.1.1 compliance) not explicitly tested | Story 1.3 |
| 4 | 🟡 Minor | 3 stories use "As a developer" persona (acceptable for embedded brownfield) | Stories 1.1, 2.1, 3.4 |
| 5 | ⚠️ Deferred | No UX design spec — screen layout/visual design left to dev agent | Deliberate; revisit post-implementation |

### Recommended Next Steps

1. **Before implementing Story 1.4 (NTP Time Sync):** Add an acceptance criterion: *"Given NTP sync has previously succeeded, when NTP connectivity is subsequently lost, then the device continues displaying time via internal RTC for at least 60 minutes before showing a time-degraded indicator."* This closes the NFR14 gap without requiring a new story.

2. **Optional — Story 3.2:** Add AC: *"Given an alarm is armed and NTP is synced, when the alarm fires, then it fires within ±1 second of the scheduled UTC time."* Closes NFR4 explicitly.

3. **Optional — Story 1.3:** Add AC: *"Given the device connects to an MQTT broker, when the connection is established, then it uses the MQTT 3.1.1 protocol and is compatible with Mosquitto, HiveMQ, and EMQX."* Closes NFR16 explicitly.

4. **Proceed to Sprint Planning** — run `/bmad-bmm-sprint-planning` in a fresh context window to sequence the 16 stories into a sprint plan.

5. **Return to UX design when ready** — run `/bmad-create-ux-design` targeting the 5 TFT screen states, then update Stories 1.2, 2.2, 3.2 with visual ACs.

### Final Note

This assessment identified **5 issues** across 3 categories (1 major gap, 3 minor gaps, 1 deliberate deferral). The major gap (NFR14) can be resolved by adding a single acceptance criterion to Story 1.4 before development begins — no new stories are required. The planning artifacts are coherent, complete, and well-structured. 40/40 FRs are traced to stories. All 4 epics deliver user value with clean dependency ordering.

---

## Document Inventory

| Document | File | Format |
|---|---|---|
| PRD | `_bmad-output/planning-artifacts/prd.md` | Whole |
| Architecture | `_bmad-output/planning-artifacts/architecture.md` | Whole |
| Epics & Stories | `_bmad-output/planning-artifacts/epics.md` | Whole |
| UX Design | N/A | Not applicable (IoT/firmware project) |
