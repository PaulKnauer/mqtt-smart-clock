# Story 2.4: Heartbeat And Event Publishing

Status: done

## Story

As an operator,
I want the device to report its health and state,
so that I can monitor it remotely.

## Acceptance Criteria

1. Given the device is running, when the heartbeat tick fires every 30 seconds, an event is pushed to the eventQueue for MQTT publication.
2. Given the device boots and MQTT connects, NetworkTask publishes "online" (retained) to clocks/state/{id}/presence and LWT publishes "offline" on disconnect.
3. Given any event is in the eventQueue, NetworkTask serializes it to JSON and publishes to the correct topic.
