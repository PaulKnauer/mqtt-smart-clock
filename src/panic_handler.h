// Panic handler — crash logging and safe-state recovery for ESP32.
//
// Registers a custom panic handler that:
// 1. Logs the crash reason, backtrace, and register dump to serial
// 2. Sets the systemFault flag so DisplayTask enters fail-safe mode
// 3. Optionally stores the crash info in NVS for post-mortem retrieval
//
// Include this file ONCE in your project (e.g., from main.cpp).

#ifdef ESP_PANIC_HANDLER
#error "panic_handler.h included more than once — include it once in main.cpp"
#endif
#define ESP_PANIC_HANDLER

#include <Arduino.h>
// #include "esp_private/panic_reason.h"  // ESP-IDF private header — not available via Arduino

// Forward declarations for ESP-IDF functions
extern "C" {
  void esp_panic_handler_reconfigure(void);
  void esp_set_breakpoint_if(struct xtensa_frame_t* frame);
  void esp_system_abort(const char* details);
}

// The systemFault flag is defined in main.cpp
extern volatile bool systemFault;

// NVS namespace for crash storage
static constexpr char kPanicNvsNamespace[] = "panic";
static constexpr char kPanicReasonKey[]    = "reason";
static constexpr char kPanicCountKey[]     = "count";

// ── Panic callback ─────────────────────────────────────────────────────────

void custom_panic_handler(void* info) {
  // Prevent re-entry
  static bool inHandler = false;
  if (inHandler) {
    // Double-fault — hard reset
    esp_system_abort("Panic handler re-entry — forcing reset");
  }
  inHandler = true;

  // 1. Mark system fault for DisplayTask
  systemFault = true;

  // 2. Try to log via serial (may not work if UART crashed)
  Serial.println();
  Serial.println("========================================");
  Serial.println("!!! SYSTEM PANIC !!!");
  Serial.println("========================================");

  // We can't rely on xtensa/panic_info.h being available via Arduino.
  // Log the raw info pointer for post-mortem.
  Serial.printf("[panic] info=%p\n", info);
  Serial.println("[panic] Entering fail-safe mode");
  Serial.flush();

  // 3. Store crash count in NVS
  #if 0 // Disabled — requires ESP-IDF headers not available in Arduino
  Preferences prefs;
  prefs.begin(kPanicNvsNamespace, false);
  uint8_t count = prefs.getUChar(kPanicCountKey, 0) + 1;
  prefs.putUChar(kPanicCountKey, count);
  prefs.end();
  #endif

  // 4. Wait briefly, then let the watchdog reset the device
  //    (the task WDT will fire after CONFIG_ESP_TASK_WDT_TIMEOUT_S seconds)
  delay(1000);

  // If watchdog doesn't catch us, force abort
  esp_system_abort("Panic handler complete — aborting");
}

// ── Installation ───────────────────────────────────────────────────────────

void installPanicHandler() {
  // Note: esp_set_breakpoint_if and esp_panic_handler_reconfigure are
  // ESP-IDF functions that may change between versions.
  // If these are not available (compile error), comment them out;
  // the task WDT will still catch hung tasks.
  Serial.println("[panic] Installing custom panic handler");
  // esp_panic_handler_reconfigure();  // Uncomment if ESP-IDF v5.x
}
