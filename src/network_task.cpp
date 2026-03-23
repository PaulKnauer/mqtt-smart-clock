#include "network_task.h"
#include <Arduino.h>

// Stub implementation. Full implementation: Story 1.3+
void networkTaskEntry(void* pvParameters) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
