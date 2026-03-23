#pragma once

// FreeRTOS queue handle declarations (firmware-only).
// Include this header in firmware task files that call xQueueSend/xQueueReceive.
// DO NOT include from native test files — it pulls in FreeRTOS headers.
//
// For native tests, include only queue_types.h (pure struct definitions).

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "queue_types.h"

// Defined in main.cpp.
extern QueueHandle_t commandQueue;  // NetworkTask → DisplayTask, capacity 8
extern QueueHandle_t eventQueue;    // DisplayTask → NetworkTask, capacity 16
