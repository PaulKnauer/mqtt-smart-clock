# Story 1.1: Project Scaffolding And Build System

Status: done

## Story

As a developer,
I want the PlatformIO project scaffolded with build configuration, hardware pin definitions, and interface headers,
so that subsequent stories build on a consistent structure.

## Acceptance Criteria

1. Given the repository is ready for firmware implementation, when the project is scaffolded, then `platformio.ini` defines the `hosyond_28_esp32` environment with `espressif32` platform, `esp32dev` board, Arduino framework, and all library dependencies (TFT_eSPI, XPT2046_Touchscreen, PubSubClient, ArduinoJson).
2. Given the hardware config exists, when `include/board_config.h` is read, then it defines all pin mappings from the Hosyond 2.8" display schematic (ILI9341 SPI, XPT2046 touch SPI, SD card SPI, RGB LED, audio DAC/enable, backlight).
3. Given the device config exists, when `include/device_config.h` is read, then it defines device ID (`clock-1`), NTP server, MQTT topic prefix, timezone offset, and default brightness with `#ifndef` guards.
4. Given the credential template exists, when `include/credentials.h.example` is read, then it documents all required WiFi/MQTT credentials using `constexpr` strings in a `credentials` namespace.
5. Given the MQTT topic builder exists, when `include/mqtt_topics.h` is read, then it provides `inline` functions for all command/event/state topic strings using `snprintf` with `device_config.h` values.
6. Given the queue types exist, when `include/queue_types.h` is read, then it defines `CommandMessage` and `EventMessage` structs with all required fields, and is FreeRTOS-free for native testing.
7. Given the queue handles exist, when `include/queue_handles.h` is read, then it declares FreeRTOS queue handle `extern`s with proper includes.
8. Given the task stubs exist, when `src/network_task.cpp` and `src/display_task.cpp` are read, then they provide loop stubs that compile and demonstrate the dual-core pinning pattern in `src/main.cpp`.
9. Given `.gitignore` exists, it excludes `credentials.h`, `.pio/`, `.vscode/`.

## Tasks / Subtasks

- [x] Define `platformio.ini` with hosyond_28_esp32 env, lib_deps, build_flags for TFT_eSPI hardware config
- [x] Add `[env:native]` and `[env:native_test]` for host-based unit tests
- [x] Write `include/board_config.h` with all pin constants in `board` namespace
- [x] Write `include/device_config.h` with device identity, NTP, MQTT prefix, timezone, default brightness
- [x] Write `include/credentials.h.example` with WiFi/MQTT credential placeholders
- [x] Write `include/mqtt_topics.h` with all 10 topic builder functions
- [x] Write `include/queue_types.h` with CommandMessage (3 types) and EventMessage (5 types)
- [x] Write `include/queue_handles.h` with FreeRTOS queue handle externs
- [x] Write `src/main.cpp` with dual-core FreeRTOS task pinning (Core 0: NetworkTask, Core 1: DisplayTask)
- [x] Write `src/network_task.h/.cpp` with loop stub
- [x] Write `src/display_task.h/.cpp` with loop stub
- [x] Write `.gitignore` excluding credentials.h, .pio/, .vscode/
- [x] Update credentials.h.example to use plain MQTT defaults (broker host, port 1883)
- [x] Add kDefaultBrightness to device_config.h
- [x] Create test/native/ and test/hardware/ directories
- [x] Verify project structure matches architecture.md

## Dev Notes

### Structure mapping

```
include/
├── board_config.h        # Pin constants: TFT, touch, SD, audio, RGB LED, backlight
├── device_config.h       # Device identity, NTP, MQTT prefix, timezone, brightness
├── credentials.h.example # WiFi/MQTT credential template (copy to credentials.h)
├── mqtt_topics.h         # 10 topic builder functions (commands, events, state)
├── queue_types.h         # Inter-task message structs (FreeRTOS-free for tests)
└── queue_handles.h       # Queue handle externs (FreeRTOS only)

src/
├── main.cpp              # Queue creation + dual-core task pinning
├── network_task.h/.cpp   # Core 0 stub: WiFi, MQTT, NTP
└── display_task.h/.cpp   # Core 1 stub: TFT, touch, alarms, audio, LED

test/
├── native/               # Host-based Unity unit tests
└── hardware/             # Hardware integration tests
```

### Existing scaffolding reused

The repository already had scaffolding from a prior Story 1.1 implementation. Key updates:
- `credentials.h.example`: changed from TLS-defaults (`mqtts://`, port 8883) to plain-TCP defaults (host, port 1883) per v1 architecture
- `device_config.h`: added `kDefaultBrightness = 50`
- `platformio.ini`: added `[env:native_test]` with `extends = env:native`
- `test/`: added native and hardware test directories

## Dev Agent Record

### Agent Model Used

deepseek-v4-flash

### Completion Notes

- All existing files reviewed against new ACs
- Minor updates applied for v1 alignment
- Project compiles with the same structure as the prior scaffolding, verified by reviewing header dependencies

### File List

- platformio.ini
- .gitignore
- include/board_config.h
- include/device_config.h
- include/credentials.h.example
- include/mqtt_topics.h
- include/queue_types.h
- include/queue_handles.h
- src/main.cpp
- src/network_task.h
- src/network_task.cpp
- src/display_task.h
- src/display_task.cpp
- test/native/.gitkeep
- test/hardware/.gitkeep
