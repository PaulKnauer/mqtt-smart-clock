// Smart Clock Firmware — main entry point
//
// FreeRTOS dual-core layout:
//   Core 0 (pro): NetworkTask @ priority 1 — WiFi, MQTT, NTP, event publishing
//   Core 1 (app): DisplayTask @ priority 5 — TFT, touch, audio, alarm evaluation
//
// Queues:
//   commandQueue (8 slots) — NetworkTask → DisplayTask (commands from MQTT)
//   eventQueue   (16 slots) — DisplayTask → NetworkTask (events to publish)

#include <Arduino.h>
#include "queue_handles.h"

// ── Queue handles (defined here, extern in queue_handles.h) ───────────────

QueueHandle_t commandQueue = nullptr;
QueueHandle_t eventQueue   = nullptr;

volatile bool ntpSynced   = false;
volatile bool systemFault = false;

// ── Task declarations ─────────────────────────────────────────────────────

void networkTaskEntry(void* pvParameters);
void displayTaskEntry(void* pvParameters);

// ── Main ──────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("[boot] Smart Clock Firmware v0.1.0");

  // Create inter-task queues
  commandQueue = xQueueCreate(8, sizeof(CommandMessage));
  eventQueue   = xQueueCreate(16, sizeof(EventMessage));

  if (!commandQueue || !eventQueue) {
    Serial.println("[boot] FATAL: Failed to create queues — halting");
    systemFault = true;
    return;
  }

  // NetworkTask on Core 0 (pro) — handles all I/O
  BaseType_t netOk = xTaskCreatePinnedToCore(
    networkTaskEntry,
    "NetworkTask",
    8192,   // stack depth
    nullptr,// params
    1,      // priority (low — periodic polling)
    nullptr,
    0       // core 0
  );

  // DisplayTask on Core 1 (app) — display, touch, audio, alarms
  BaseType_t dispOk = xTaskCreatePinnedToCore(
    displayTaskEntry,
    "DisplayTask",
    8192,   // stack depth
    nullptr,
    5,      // priority (high — responsive UI)
    nullptr,
    1       // core 1
  );

  if (netOk != pdPASS || dispOk != pdPASS) {
    Serial.println("[boot] FATAL: Task creation failed — halting");
    systemFault = true;
    return;
  }

  Serial.println("[boot] Tasks created, starting scheduler");
}

void loop() {
  // Arduino framework requires loop() even with FreeRTOS.
  // Delete it so the idle task gets CPU.
  vTaskDelete(nullptr);
}
