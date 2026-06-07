# Story 3.1: Alarm Evaluation And Triggering

Status: done

## Story

As a user,
I want the alarm to ring at the correct time,
so that I am notified.

## Acceptance Criteria

1. Given an alarm is stored in NVS, when the RTC time matches the alarm time, then DisplayTask switches to alarm_ringing mode, activates audio buzzer via ledc on GPIO26 DAC, pulses display red, and pushes alarm_triggered event.
2. Given the alarm has been ringing for 60 seconds without dismiss, it auto-stops and publishes alarm_acknowledged with action "timeout".
