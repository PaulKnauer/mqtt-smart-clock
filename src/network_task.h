#pragma once

// NetworkTask: runs on Core 0 (proto core), priority 1 (normal).
// Owns: Wi-Fi management, MQTT connect/reconnect, NTP sync, inbound message receipt.
//
// Cross-task rules (AR6):
// - NEVER access the tft instance from NetworkTask
// - All outbound commands are pushed to commandQueue (parsed from MQTT)
// - All inbound events are drained from eventQueue and published via MQTT
//
// Full implementation: Story 1.3 (connectivity) and Story 1.4+ (NTP, heartbeat)

void networkTaskEntry(void* pvParameters);
