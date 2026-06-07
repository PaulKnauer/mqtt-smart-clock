# Story 2.1: Display State Machine And Message Overlay

Status: done

## Story

As a user,
I want messages displayed on the clock when received,
so that I can see reminders and notifications.

## Acceptance Criteria

1. Given a `display_message` command arrives on the commandQueue, when DisplayTask processes it, then the display switches to message overlay mode showing the message text centered on screen with an auto-dismiss timer.
2. Given a new message arrives while one is already showing, when processed, the display updates to the new message and resets the timer.
3. Given the display state machine exists, display modes follow priority: alarm_ringing > message > clock > error.
