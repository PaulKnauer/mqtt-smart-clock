# Story 2.2: Set Brightness Command

Status: done

## Story

As a user,
I want to adjust the display brightness remotely,
so that it's comfortable day and night.

## Acceptance Criteria

1. Given a `set_brightness` command arrives on the commandQueue, when processed, it clamps level 0-100, adjusts backlight PWM via ledc on GPIO21, persists level in NVS, returns command_result "applied".
2. Given level is 0, when processed, the backlight turns off.
