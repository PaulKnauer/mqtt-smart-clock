# Code Review: Story 1.1

- Story: `1-1-project-scaffolding-and-build-system`
- Review mode: `full`
- Scope: current working tree changes for the Story 1.1 implementation files
- Spec/context reviewed:
  - `_bmad-output/implementation-artifacts/1-1-project-scaffolding-and-build-system.md`
  - `docs/lcd-reference.md`
- Build verification:
  - `pio run -e hosyond_28_esp32` succeeded on 2026-03-23
  - Result: RAM 6.4%, Flash 17.8%
- Review note:
  - The BMAD workflow expects parallel subagents. In this session, the same review lenses were executed locally and then triaged into the BMAD categories below.

## Bad Spec

These findings suggest the spec should be amended. Consider regenerating or amending the spec with this context:

1. Presence topic is specified under the wrong namespace
   - Detail: The story spec and implementation guide classify `presence` as an event topic and show it under `clocks/events/...`, but the project contract defines presence as retained state under `clocks/state/{device_id}/presence`. The current code follows the story, so the story is the source of the mismatch.
   - Evidence:
     - Story AC5 lists `presence` under event topics in `_bmad-output/implementation-artifacts/1-1-project-scaffolding-and-build-system.md:88`
     - Story implementation guide shows `clocks/events/%s/presence` in `_bmad-output/implementation-artifacts/1-1-project-scaffolding-and-build-system.md:225`
     - Code implements `eventPresence()` as `.../events/.../presence` in `include/mqtt_topics.h:48`
     - Contract defines presence as `clocks/state/{device_id}/presence` in `docs/lcd-reference.md:176`
   - Suggested spec amendment: Move presence from the event list into retained state topics and update the story’s MQTT topic examples accordingly.

2. AC2 requires a PlatformIO dependency spec that the registry does not resolve
   - Detail: AC2 requires `paulstoffregen/XPT2046_Touchscreen@^1.4`, but the implementation record already documents that this package spec fails to resolve in PlatformIO and the buildable form is the GitHub URL. The code builds successfully with the GitHub source, so the spec is stricter than the toolchain currently allows.
   - Evidence:
     - AC2 requires the registry package form in `_bmad-output/implementation-artifacts/1-1-project-scaffolding-and-build-system.md:76`
     - The story debug log records `UnknownPackageError` for that exact package spec in `_bmad-output/implementation-artifacts/1-1-project-scaffolding-and-build-system.md:32`
     - The working implementation uses the GitHub URL in `platformio.ini:12`
   - Suggested spec amendment: Accept either the GitHub URL pinned to the upstream repo or the resolved package version actually supported by PlatformIO, instead of requiring the currently non-resolving registry identifier.

## Patch

These are fixable code issues:

1. Shared credentials contract diverges from the story and transport contract
   - Detail: The story template defines `kMqttBrokerUrl` with an `mqtts://...` value, but the checked-in example changed that field to `kMqttBrokerHost`. That creates a shared-contract mismatch before Story 1.3 starts, because future connectivity code and docs now have two incompatible source-of-truths for the broker setting shape.
   - Location: `include/credentials.h.example:11`
   - Evidence:
     - Story template requires `kMqttBrokerUrl` in `_bmad-output/implementation-artifacts/1-1-project-scaffolding-and-build-system.md:159`
     - The device reference says broker URLs should normally use `mqtts://` in `docs/lcd-reference.md:59`

2. `queue_types.h` is not usable from native tests despite the story requiring shared test access
   - Detail: The story explicitly says this header lives in `include/` so future `test/` files can include it, and also says the native environment must avoid Arduino and ESP32 headers. The implementation pulls in FreeRTOS headers directly, so any host-side test that includes `queue_types.h` under `[env:native]` will fail before tests can even compile.
   - Location: `include/queue_types.h:8`
   - Evidence:
     - `queue_types.h` includes `<freertos/FreeRTOS.h>` and `<freertos/queue.h>` in `include/queue_types.h:8`
     - The story says native tests have no Arduino or ESP32 headers in `_bmad-output/implementation-artifacts/1-1-project-scaffolding-and-build-system.md:146`
     - The story says this header is placed in `include/` so future test files can access it in `_bmad-output/implementation-artifacts/1-1-project-scaffolding-and-build-system.md:251`

## Summary

0 intent_gap, 2 bad_spec, 2 patch, 0 defer findings. 0 findings rejected as noise.

## Recommended Next Steps

- Amend the story so the MQTT presence topic matches `docs/lcd-reference.md` and the XPT2046 dependency requirement matches what PlatformIO can actually resolve.
- Fix `include/credentials.h.example` before Story 1.3 consumes it.
- Decouple `queue_types.h` from FreeRTOS types, or add a native-safe abstraction so host tests can include the shared contract header.
