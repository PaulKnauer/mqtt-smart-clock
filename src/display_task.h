#pragma once

// DisplayTask: runs on Core 1 (app core), priority 5 (high).
// Owns: TFT rendering, display state machine, touch polling, RGB LED, audio, alarm evaluation.
//
// Cross-task rules (AR6):
// - NEVER call blocking network functions from DisplayTask
// - NEVER access the tft instance from NetworkTask
// - All inbound commands arrive via commandQueue
// - All outbound events depart via eventQueue

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "queue_handles.h"

// Display mode priority order (highest to lowest)
enum class DisplayMode {
  kAlarmRinging,   // highest priority — alarm active
  kMessage,        // transient message overlay
  kClock,          // default idle state
  kError           // connectivity/time lost
};

class DisplayManager {
public:
  DisplayManager();
  void begin();
  void loop();

  static DisplayManager* instance;

private:
  TFT_eSPI tft_;

  // Display state
  DisplayMode currentMode_;
  DisplayMode previousMode_;

  // Message overlay
  char overlayMessage_[160];
  unsigned long overlayStartMs_;
  unsigned long overlayDurationMs_;

  // Brightness
  uint8_t brightnessLevel_;
  void setupBacklight();
  void setBacklight(uint8_t level);
  void persistBrightness(uint8_t level);
  uint8_t loadBrightness();

  // Clock face
  void renderClockFace();
  void renderMessageOverlay();
  void renderAlarmRinging();
  int lastMinute_;  // only redraw when minute changes

  // Command processing
  void processCommandQueue();
  void handleSetBrightness(const CommandMessage& cmd);
  void handleDisplayMessage(const CommandMessage& cmd);
  void handleSetAlarm(const CommandMessage& cmd);

  // Event helpers
  void sendCommandResult(const char* commandType, const char* status, const char* detail);
  void sendDisplayState();
  void sendAlarmState(bool armed);

  // Preferences (NVS)
  void initNvs();
  unsigned long millis() const;
};

void displayTaskEntry(void* pvParameters);
