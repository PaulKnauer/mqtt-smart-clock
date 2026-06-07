# Story 1.2: NetworkTask With WiFi, MQTT, And NTP

Status: in-progress

## Story

As a user,
I want the device to connect to WiFi and MQTT automatically on boot and stay connected,
so that it can receive commands without manual setup.

## Acceptance Criteria

1. Given valid WiFi and MQTT credentials are configured, when the device boots, then it connects to WiFi within 15 seconds, connects to MQTT broker, subscribes to `clocks/commands/{deviceId}/#`, publishes `online` (retained) to `clocks/state/{deviceId}/presence`, and syncs time from NTP. NetworkTask runs on Core 0 with stack size 8192 and priority 1.
2. Given WiFi drops after initial connection, when the network is lost, then NetworkTask retries WiFi every 5 seconds until reconnected, then reconnects MQTT and resubscribes.
3. Given MQTT broker drops after initial connection, when the connection is lost, then NetworkTask retries MQTT every 5 seconds until reconnected, resubscribes to command topics. LWT publishes `offline` to `clocks/state/{deviceId}/presence`.
4. Given NTP sync fails on first boot, when the network is available, then it retries NTP sync every 30 seconds until successful.
5. Given a valid MQTT message arrives on a command topic, when the callback fires, then it parses topic + JSON payload, validates deviceId, creates a CommandMessage, and pushes it to commandQueue (non-blocking). Invalid JSON or mismatched deviceId results in a logged warning — no crash.
6. Given the eventQueue contains an EventMessage, when NetworkTask polls the queue, then it serializes the event data to JSON and publishes to the specified MQTT topic with correct QoS and retain flags.
