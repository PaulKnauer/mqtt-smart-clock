# Story 1.3: Clock Face Display

Status: in-progress

## Story

As a user,
I want to see the current time on a readable clock face,
so that the device functions as a desk clock.

## Acceptance Criteria

1. Given NTP has synced time and the RTC is running, when DisplayTask renders the clock face, then it displays HH:MM in large digits centered on the screen, SS in smaller digits, and the date + day of week below. The display updates every second.
2. Given the display hardware is initialized, when DisplayTask runs, then TFT_eSPI is initialised with the correct pin configuration from `board_config.h`, backlight is set to the default level (50%) on first boot or the persisted level on subsequent boots, and DisplayTask runs on Core 1 with stack size 8192 and priority 5.
3. Given the backlight level is changed, when the new level is set, then it is persisted in NVS and restored on next boot.
4. Given no commands or alarms are active, when DisplayTask renders, then it remains in clock face mode (lowest priority, default state).
