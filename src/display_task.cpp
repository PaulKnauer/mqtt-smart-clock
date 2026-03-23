#include "display_task.h"
#include <Arduino.h>

// Stub implementation. Full implementation: Story 1.2+
void displayTaskEntry(void* pvParameters) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
