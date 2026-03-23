#pragma once

// DisplayTask: runs on Core 1 (app core), priority 5 (high).
// Owns: TFT rendering, display state machine, touch polling, RGB LED, audio, alarm evaluation.
//
// Cross-task rules (AR6):
// - NEVER call blocking network functions from DisplayTask
// - NEVER access the tft instance from NetworkTask
// - All inbound commands arrive via commandQueue
// - All outbound events depart via eventQueue
//
// Full implementation: Story 1.2 (display) and Story 1.3+ (queue processing)

void displayTaskEntry(void* pvParameters);
