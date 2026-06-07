#pragma once

// DisplayTask: runs on Core 1 (app core), priority 5 (high).
// Owns: TFT rendering, display state machine, touch polling, RGB LED, audio, alarm evaluation.

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "queue_handles.h"

enum class DisplayMode {
  kAlarmRinging,
  kMessage,
  kClock,
  kError
};

class DisplayManager {
public:
  DisplayManager();
  void begin();
  void loop();

  static DisplayManager* instance;

private:
  TFT_eSPI tft_;
  XPT2046_Touchscreen touch_;

  DisplayMode currentMode_;
  DisplayMode previousMode_;

  char overlayMessage_[160];
  unsigned long overlayStartMs_;
  unsigned long overlayDurationMs_;

  uint8_t brightnessLevel_;
  void setupBacklight();
  void setBacklight(uint8_t level);
  void persistBrightness(uint8_t level);
  uint8_t loadBrightness();
  void initNvs();

  void renderClockFace();
  void showMessage(const char* msg, unsigned long durationMs);
  void renderAlarmRinging();
  int lastMinute_;

  void processCommandQueue();

  // Alarm
  time_t storedAlarmTime_;
  char storedAlarmLabel_[64];
  unsigned long alarmStartMs_;
  bool alarmRinging_;
  void saveAlarm(time_t alarmTime, const char* label);
  void clearAlarm();
  void loadAlarm();
  void evaluateAlarm(time_t now);
  void stopAlarm(const char* action);

  void sendCommandResult(const char* commandType, const char* status, const char* detail);
  void sendDisplayState();
  void sendAlarmState(bool armed);
  unsigned long millis() const;
};

void displayTaskEntry(void* pvParameters);
