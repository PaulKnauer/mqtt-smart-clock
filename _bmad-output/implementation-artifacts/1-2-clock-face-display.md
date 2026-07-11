# Story 1.2: Clock Face Display

**Epic:** 1 — Connected Clock Foundation
**Story ID:** 1.2
**Status:** ready-for-dev
**Created:** 2026-03-23

---

## Tasks / Subtasks

- [ ] Task 1: Create `src/display_state.h` — `DisplayMode` enum + `DisplayState` struct
- [ ] Task 2: Update `src/display_task.h` — proper header replacing stub
- [ ] Task 3: Implement `src/display_task.cpp` — full DisplayTask with clock face, time-unavailable, and error screens
- [ ] Task 4: Verify `pio run -e hosyond_28_esp32` builds successfully

---

## Dev Agent Record

### Implementation Plan
_Populated during implementation._

### Debug Log
_Populated during implementation._

### Completion Notes
_Populated during implementation._

---

## File List
_Populated during implementation._

---

## Change Log
_Populated during implementation._

---

## User Story

As Paul,
I want the device TFT to show a clock face with time and date updating every second,
So that the device functions as a visible clock from the moment it powers on.

---

## Acceptance Criteria

**AC1 — Clock face renders on boot:**
Given the device is powered on, when DisplayTask initializes the TFT,
Then a clock face renders showing time in HH:MM:SS format and the current date in 320×240 landscape orientation.

**AC2 — Time updates every second without flicker:**
Given the clock face is displayed, when 1 second elapses,
Then the displayed time updates with no visible flicker, tearing, or lag.

**AC3 — Time-unavailable indicator when NTP not synced:**
Given no NTP sync has yet occurred, when the clock face state is active,
Then a time-unavailable indicator is shown in place of the time digits.

**AC4 — Display loop never blocks on network I/O:**
Given DisplayTask is running on Core 1, when the display loop executes,
Then it never waits on any network I/O and continues rendering independently of NetworkTask state.

**AC5 — Error screen is distinct:**
Given the display enters an unrecoverable error state, when the error condition is detected,
Then an error indicator screen is rendered, distinct from the clock face and time-unavailable screens.

---

## Critical Developer Context

### What Already Exists — Build On, Don't Reinvent

**From Story 1.1:**
- `include/board_config.h` — all GPIO constants (`board::kLedRed`, `board::kAudioEnable`, `board::kTftBacklight`, `board::kTouchIrq`, etc.) — never redefine these
- `include/queue_handles.h` — FreeRTOS queue handles (`commandQueue`, `eventQueue`) — include this for `xQueueReceive`
- `include/queue_types.h` — `CommandMessage` + `EventMessage` structs
- `src/display_task.cpp/.h` — **stub** — this story replaces these stubs with the real implementation
- `src/main.cpp` — already creates queues and calls `xTaskCreatePinnedToCore(displayTaskEntry, ...)` — **do not modify**
- `platformio.ini` — `TFT_eSPI @ ^2.5.43` already in lib_deps; all TFT SPI pins defined in `build_flags`

**TFT configuration (already wired via build_flags):**
| Signal | GPIO |
|---|---|
| CS | 15 |
| DC | 2 |
| SCK | 14 |
| MOSI | 13 |
| MISO | 12 |
| BL (backlight) | 21 |
- Driver: ILI9341 via `USER_SETUP_LOADED=1` + `ILI9341_DRIVER=1` build flags
- TFT_eSPI uses these build flags automatically — do NOT pass SPI pins to `TFT_eSPI()` constructor; use `TFT_eSPI tft = TFT_eSPI()` with no args

### Code Conventions

Follow exactly (established by `board_config.h`):
- Constants: `kPascalCase` in namespaces — **never** `ALL_CAPS` macros
- Files: `snake_case.h/.cpp`
- Static file-scope helpers: use `namespace { ... }` anonymous namespace
- Board GPIO always via `board::k*` constants, not magic numbers

### Where GPIO Init Now Lives

The old bring-up `src/main.cpp` had all GPIO `pinMode`/`digitalWrite` calls in `setup()`. That code was deleted in Story 1.1. **DisplayTask is now responsible for initialising all display-side GPIO** in its startup section (before entering the render loop). NetworkTask handles no GPIO.

Display-side GPIO to init in `displayTaskEntry` startup:
- `board::kTftBacklight` (21) — `OUTPUT`, set `HIGH`
- `board::kLedRed` (22), `kLedGreen` (16), `kLedBlue` (17) — `OUTPUT`, set `HIGH` (common-anode LEDs: HIGH = off)
- `board::kAudioEnable` (4) — `OUTPUT`, set `HIGH` (active-low amplifier: HIGH = muted)
- `board::kTouchIrq` (36) — `INPUT`

---

## Implementation Guide

### File 1: `src/display_state.h` (new)

```cpp
#pragma once

#include <cstdint>

// Display mode priority (higher value = higher priority).
// Priority order: kAlarmRinging > kMessage > kClock > kError > kTimeUnavailable
// A lower-priority state cannot override a higher-priority active state.
// Stories 2.2 (kMessage) and 3.2 (kAlarmRinging) add their modes here — do not change values.
enum class DisplayMode : uint8_t {
  kTimeUnavailable = 0,  // NTP not yet synced — no valid time to show
  kError           = 1,  // unrecoverable display error
  kClock           = 2,  // normal clock face (default operating state)
  kMessage         = 3,  // transient message overlay — Story 2.2
  kAlarmRinging    = 4   // alarm active — Story 3.2, highest priority
};

// Shared display state. Owned by DisplayTask; updated only from DisplayTask context.
// Extended by later stories (message text, alarm label, brightness, etc.).
struct DisplayState {
  DisplayMode mode    = DisplayMode::kTimeUnavailable;
  bool        ntpSynced = false;
};
```

### File 2: `src/display_task.h` (replace stub)

```cpp
#pragma once

// DisplayTask: runs on Core 1 (app core), priority 5 (high).
// Owns: TFT rendering, display state machine, touch polling, RGB LED, audio, alarm evaluation.
//
// Cross-task rules (AR6):
// - NEVER call blocking network functions from DisplayTask
// - NEVER access the tft instance from NetworkTask
// - All inbound commands arrive via commandQueue (queue_handles.h)
// - All outbound events depart via eventQueue (queue_handles.h)

void displayTaskEntry(void* pvParameters);
```

### File 3: `src/display_task.cpp` (replace stub)

#### Structure overview

```
displayTaskEntry()
├── Hardware init (GPIO, Serial, TFT)
├── Initial screen render (kTimeUnavailable — NTP not synced yet)
└── Render loop (every 100 ms):
    ├── Drain commandQueue (no-op until Story 2.1)
    ├── Poll ntpSynced via getLocalTime()
    ├── Transition display mode if changed
    └── Tick clock update every second (clock mode only)
```

#### Required includes

```cpp
#include "display_task.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "board_config.h"
#include "queue_handles.h"   // commandQueue, eventQueue (FreeRTOS handles)
#include "display_state.h"
```

#### TFT instance — file-scope, anonymous namespace

```cpp
namespace {
  TFT_eSPI tft = TFT_eSPI();  // no constructor args — SPI pins from build_flags
}
```

#### Hardware init block (runs once at task start)

```cpp
Serial.begin(115200);
delay(100);  // Arduino delay() is safe here — tasks haven't yielded yet

// Backlight on
pinMode(board::kTftBacklight, OUTPUT);
digitalWrite(board::kTftBacklight, HIGH);

// RGB LED — all off (common-anode: HIGH = off)
pinMode(board::kLedRed,   OUTPUT);  digitalWrite(board::kLedRed,   HIGH);
pinMode(board::kLedGreen, OUTPUT);  digitalWrite(board::kLedGreen, HIGH);
pinMode(board::kLedBlue,  OUTPUT);  digitalWrite(board::kLedBlue,  HIGH);

// Audio amplifier — muted (active-low: HIGH = muted)
pinMode(board::kAudioEnable, OUTPUT);
digitalWrite(board::kAudioEnable, HIGH);

// Touch IRQ (read only)
pinMode(board::kTouchIrq, INPUT);

// TFT init
tft.init();
tft.setRotation(1);   // landscape 320×240
tft.fillScreen(TFT_BLACK);
```

#### Display helper functions (static, anonymous namespace)

**`drawTimeUnavailable()`** — shown before NTP sync (AC3):
```
Background: TFT_BLACK (fillScreen on first call; partial clear on transition)
Center: "--:--:--"   Font 7 (48px 7-segment style), TFT_DARKGREY on TFT_BLACK
Below:  "Waiting for NTP sync"  Font 2 (small), TFT_YELLOW on TFT_BLACK
Coords: both centered, MC_DATUM at x=160, y=105 (time) and y=165 (label)
```

**`drawClockFace(const struct tm& t)`** — full clock screen render (mode transition):
```
Background: TFT_BLACK (fillScreen)
Time:  "HH:MM:SS"   Font 7, TFT_WHITE on TFT_BLACK, MC_DATUM x=160 y=105
Date:  "Mon, 23 Mar 2026"  Font 4 (medium), TFT_CYAN on TFT_BLACK, MC_DATUM x=160 y=165
```

**`updateClockTime(const struct tm& t)`** — called every second in clock mode (AC2):
```
Redraws ONLY the time string (Font 7, MC_DATUM x=160 y=105).
Key: call tft.setTextColor(TFT_WHITE, TFT_BLACK) — the background param causes TFT_eSPI
to automatically overwrite the previous text bounding box without a fillRect.
This is the flicker-free technique: no screen clear needed.
```

**`drawError(const char* detail)`** — unrecoverable error (AC5):
```
Background: TFT_BLACK (fillScreen)
Line 1: "ERROR"  Font 4, TFT_RED on TFT_BLACK, MC_DATUM x=160 y=95
Line 2: detail   Font 2, TFT_WHITE on TFT_BLACK, MC_DATUM x=160 y=130
(detail truncated to 40 chars if needed)
```

#### Render loop

```cpp
DisplayState state;
struct tm timeInfo = {};
uint8_t lastSecond = 0xFF;  // sentinel — forces update on first clock tick

// Show time-unavailable immediately (NTP not synced at this point)
drawTimeUnavailable();

for (;;) {
  // ── Drain command queue (no-op in Story 1.2; handlers added in Story 2.1) ──
  CommandMessage cmd;
  while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
    // TODO Story 2.1: dispatch to command handlers
  }

  // ── NTP sync detection (Story 1.4 will configure NTP; getLocalTime returns
  //    false until configTime() has been called and time has been set) ──────
  bool ntpSynced = getLocalTime(&timeInfo);

  // ── Mode transitions ──────────────────────────────────────────────────────
  DisplayMode desired = ntpSynced ? DisplayMode::kClock : DisplayMode::kTimeUnavailable;

  if (desired != state.mode) {
    state.mode = desired;
    lastSecond = 0xFF;  // force redraw on next loop
    if (state.mode == DisplayMode::kClock) {
      drawClockFace(timeInfo);
    } else {
      drawTimeUnavailable();
    }
  }

  // ── Per-second clock tick (clock mode only) ───────────────────────────────
  if (state.mode == DisplayMode::kClock && timeInfo.tm_sec != lastSecond) {
    lastSecond = timeInfo.tm_sec;
    updateClockTime(timeInfo);
  }

  vTaskDelay(pdMS_TO_TICKS(100));  // 100 ms loop — never blocks on network (AC4)
}
```

#### String formatting helpers

```cpp
// Format time string "HH:MM:SS"
char timeBuf[9];
snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
         timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

// Format date string "Mon, 23 Mar 2026"
char dateBuf[20];
const char* dayNames[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char* monthNames[] = {"Jan","Feb","Mar","Apr","May","Jun",
                             "Jul","Aug","Sep","Oct","Nov","Dec"};
snprintf(dateBuf, sizeof(dateBuf), "%s, %02d %s %04d",
         dayNames[timeInfo.tm_wday],
         timeInfo.tm_mday,
         monthNames[timeInfo.tm_mon],
         timeInfo.tm_year + 1900);
```

#### Error state entry

Wrap `tft.init()` in an error check. If TFT init fails (unlikely but architecture requires the error state), call `drawError("TFT init failed")` and loop forever:

```cpp
tft.init();
// TFT_eSPI does not return an error code; a watchdog reset handles
// hard hardware faults (NFR12). For soft display errors detected later,
// call drawError() and vTaskDelay-loop to show the screen.
```

---

## Display Layout Reference

```
┌──────────────────────────────────────┐  320×240 landscape (rotation=1)
│                                      │
│                                      │  y=105
│            HH:MM:SS                  │  Font 7 (48px 7-seg), white, centered x=160
│                                      │
│          Mon, 23 Mar 2026            │  y=165  Font 4 (26px), cyan, centered x=160
│                                      │
│   [next alarm area — Story 3.1]      │  y=210  (reserve space; don't draw yet)
└──────────────────────────────────────┘
```

```
Time-unavailable:
  y=105  "--:--:--"           Font 7, TFT_DARKGREY
  y=165  "Waiting for NTP sync"  Font 2, TFT_YELLOW

Error:
  y=95   "ERROR"              Font 4, TFT_RED
  y=130  <detail>             Font 2, TFT_WHITE
```

---

## Architecture Guardrails

**MUST follow:**
- `displayTaskEntry` owns all `tft.*` calls — NetworkTask must never touch `tft`
- `vTaskDelay(pdMS_TO_TICKS(100))` at loop end — never `delay()` in a task loop
- Use `xQueueReceive(..., 0)` with 0 timeout — never block waiting on commandQueue
- `setTextColor(fg, bg)` with background param — the way to avoid flicker without fillRect
- All GPIO via `board::k*` constants — never magic GPIO numbers
- `TFT_eSPI tft = TFT_eSPI()` — no constructor args (pins via build_flags)

**Do NOT implement in this story:**
- Touch polling (Story 3.3)
- RGB LED patterns (Story 3.2)
- Audio (Story 3.2)
- commandQueue command dispatch (Story 2.1)
- Next alarm display area (Story 3.1)
- Brightness control (Story 2.3)

**Cross-story contract established here:**
- `DisplayMode` enum values — Stories 2.2 and 3.2 add `kMessage` and `kAlarmRinging` handling; **do not change existing values or ordering**
- `DisplayState` struct — extended in Stories 2.2, 2.3, 3.1

---

## Verification

After implementation:
1. `pio run -e hosyond_28_esp32` succeeds
2. `src/display_task.cpp` has no `delay()` calls in the render loop (only `vTaskDelay`)
3. `src/display_task.cpp` does not include any Wi-Fi, MQTT, or network headers
4. `drawTimeUnavailable()` and `drawClockFace()` are distinct screens (AC5 equivalent for all 3 modes)
5. Time update uses `setTextColor(fg, bg)` with both fg and bg params — not `fillRect` + redraw

---

## Dev Notes

_To be filled in by the developer during/after implementation._
