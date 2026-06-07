# Story 2.3: Set Alarm Command

Status: done

## Story

As a user,
I want to set an alarm on the clock remotely,
so that I can be woken or reminded at a specific time.

## Acceptance Criteria

1. Given a `set_alarm` command arrives on the commandQueue, when DisplayTask processes it, it parses the RFC3339 alarm time, stores in NVS for persistence across reboots, and publishes command_result "applied".
2. Given the alarm time is invalid (0), when processed, it publishes command_result "rejected" with reason "invalid time".
3. Given a new alarm arrives when one is already set, it replaces the existing alarm.
