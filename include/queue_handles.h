#pragma once

// /!\ Shared state between NetworkTask (Core 0) and DisplayTask (Core 1) /!\
//
// Rules:
// 1. NetworkTask WRITES to these flags; DisplayTask only READS.
// 2. Writes are atomic on ESP32 (word-aligned single bytes).
// 3. No mutex required — single-writer, single-reader.

#include <Arduino.h>
#include <queue_types.h>

// FreeRTOS queues for cross-task IPC
extern QueueHandle_t commandQueue;
extern QueueHandle_t eventQueue;

// NTP sync status — written by NetworkTask, read by DisplayTask.
// DisplayTask will NOT evaluate alarms until this is true.
extern volatile bool ntpSynced;

// Fail-safe mode flag — set when a critical error is detected.
// Currently set by panic handler, read by DisplayTask.
extern volatile bool systemFault;
