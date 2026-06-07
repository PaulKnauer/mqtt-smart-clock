# Story 3.2: Touch Dismiss And Snooze

Status: done

## Story

As a user,
I want to silence the alarm by touching the screen,
so that it stops ringing when I'm awake.

## Acceptance Criteria

1. Given the display is in alarm_ringing mode, when the user touches the "Dismiss" button, then the alarm stops, display returns to clock face, and an `alarm_acknowledged` event with action "dismiss" is published.
2. Given the display is in alarm_ringing mode, when the user touches the "Snooze (5 min)" button, then the alarm stops, is re-armed for 5 minutes, and an `alarm_acknowledged` event with action "snooze" is published.
3. Given the XPT2046 touch controller is initialized, when a touch occurs, coordinates are mapped to button regions and debounced with a minimum interval.
