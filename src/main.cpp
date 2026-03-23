#include <Arduino.h>
#include "queue_handles.h"
#include "display_task.h"
#include "network_task.h"

// Queue handles — defined here, extern-declared in queue_handles.h (AR1).
QueueHandle_t commandQueue;  // NetworkTask → DisplayTask, capacity 8
QueueHandle_t eventQueue;    // DisplayTask → NetworkTask, capacity 16

void setup() {
  commandQueue = xQueueCreate(8, sizeof(CommandMessage));
  eventQueue   = xQueueCreate(16, sizeof(EventMessage));

  // DisplayTask on Core 1 (app core), priority 5 (high).
  // Owns: TFT, display state machine, touch, alarm evaluation, audio, RGB LED (AR5, AR6).
  xTaskCreatePinnedToCore(
    displayTaskEntry,  // task function
    "DisplayTask",     // task name
    8192,              // stack size in bytes
    nullptr,           // task parameter
    5,                 // priority (high)
    nullptr,           // task handle (not needed)
    1                  // core 1
  );

  // NetworkTask on Core 0 (proto core), priority 1 (normal).
  // Owns: Wi-Fi, MQTT connect/reconnect/subscribe, NTP sync, inbound message parse (AR5, AR6).
  xTaskCreatePinnedToCore(
    networkTaskEntry,  // task function
    "NetworkTask",     // task name
    8192,              // stack size in bytes
    nullptr,           // task parameter
    1,                 // priority (normal)
    nullptr,           // task handle (not needed)
    0                  // core 0
  );
}

// FreeRTOS tasks own all work. The Arduino loop task is deleted immediately.
void loop() {
  vTaskDelete(nullptr);
}
