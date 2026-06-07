#include "display_task.h"
#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "board_config.h"
#include "device_config.h"
#include "queue_handles.h"

DisplayManager* DisplayManager::instance = nullptr;

// Display dimensions
constexpr int kScreenWidth  = 320;
constexpr int kScreenHeight = 240;

// Clock face layout
constexpr int kTimeY   = 80;   // Y position of HH:MM
constexpr int kSecX    = 220;  // X position of :SS
constexpr int kDateY   = 140;  // Y position of date line
constexpr int kMsgY    = 100;  // Y position of message overlay text

// Colors (16-bit)
constexpr uint16_t kColorBg       = TFT_BLACK;
constexpr uint16_t kColorTime     = TFT_WHITE;
constexpr uint16_t kColorSeconds  = TFT_SKYBLUE;
constexpr uint16_t kColorDate     = TFT_LIGHTGREY;
constexpr uint16_t kColorMessage  = TFT_YELLOW;
constexpr uint16_t kColorAlarm    = TFT_RED;

// Backlight PWM
constexpr int kPwmChannel   = 0;
constexpr int kPwmFreq      = 5000;
constexpr int kPwmResolution = 8;  // 0-255

// NVS
constexpr char kNvsNamespace[]     = "clock";
constexpr char kNvsBrightnessKey[] = "brightness";

// Overlay defaults
constexpr unsigned long kOverlayDefaultDurationMs = 10000;

// Font sizes used
// TFT_eSPI fonts: 4 = 26pt, 6 = 48pt, 7 = 28pt, 8 = 12pt

// ── Construction ─────────────────────────────────────────────────────────────

DisplayManager::DisplayManager()
  : currentMode_(DisplayMode::kClock),
    previousMode_(DisplayMode::kClock),
    overlayStartMs_(0),
    overlayDurationMs_(0),
    brightnessLevel_(device_config::kDefaultBrightness),
    lastMinute_(-1) {
  instance = this;
  memset(overlayMessage_, 0, sizeof(overlayMessage_));
}

void DisplayManager::begin() {
  initNvs();

  // Initialise TFT
  tft_.init();
  tft_.setRotation(1);  // Landscape (320 wide x 240 tall)
  tft_.fillScreen(kColorBg);

  // Backlight
  setupBacklight();
  brightnessLevel_ = loadBrightness();
  setBacklight(brightnessLevel_);
}

void DisplayManager::loop() {
  processCommandQueue();

  time_t now = time(nullptr);
  struct tm timeinfo;

  // Only redraw clock once per minute (unless message/alarm)
  if (currentMode_ == DisplayMode::kClock) {
    if (!getLocalTime(&timeinfo, 0)) return;  // NTP not synced yet
    int currentMinute = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    if (currentMinute != lastMinute_) {
      lastMinute_ = currentMinute;
      renderClockFace();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    return;
  }

  // Message mode — check for auto-dismiss
  if (currentMode_ == DisplayMode::kMessage) {
    if (millis() - overlayStartMs_ >= overlayDurationMs_) {
      currentMode_ = DisplayMode::kClock;
      tft_.fillScreen(kColorBg);
      lastMinute_ = -1;  // force redraw
      sendDisplayState();
    }
  }

  // Alarm ringing — rendered each tick (updates every loop)
  if (currentMode_ == DisplayMode::kAlarmRinging) {
    renderAlarmRinging();
  }

  vTaskDelay(pdMS_TO_TICKS(200));
}

// ── Backlight ────────────────────────────────────────────────────────────────

void DisplayManager::setupBacklight() {
  ledcSetup(kPwmChannel, kPwmFreq, kPwmResolution);
  ledcAttachPin(board::kTftBacklight, kPwmChannel);
}

void DisplayManager::setBacklight(uint8_t level) {
  // Scale 0-100 to 0-255
  uint8_t pwm = (level > 100) ? 255 : (level * 255) / 100;
  ledcWrite(kPwmChannel, pwm);
}

void DisplayManager::persistBrightness(uint8_t level) {
  Preferences prefs;
  prefs.begin(kNvsNamespace, false);
  prefs.putUChar(kNvsBrightnessKey, level);
  prefs.end();
}

uint8_t DisplayManager::loadBrightness() {
  Preferences prefs;
  prefs.begin(kNvsNamespace, true);
  uint8_t level = prefs.getUChar(kNvsBrightnessKey, device_config::kDefaultBrightness);
  prefs.end();
  return level;
}

// ── NVS ──────────────────────────────────────────────────────────────────────

void DisplayManager::initNvs() {
  Preferences prefs;
  prefs.begin(kNvsNamespace, false);
  prefs.end();
}

// ── Clock Face ───────────────────────────────────────────────────────────────

void DisplayManager::renderClockFace() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  if (!localtime_r(&now, &timeinfo)) return;

  // Clear screen
  tft_.fillScreen(kColorBg);

  // Time: HH:MM in large font
  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  tft_.setTextColor(kColorTime, kColorBg);
  tft_.setTextSize(0);  // use font number
  tft_.setFreeFont(&FreeSansBold24pt7b);   // large bold digits

  // Centre the time string
  int timeWidth  = tft_.textWidth(timeStr);
  int timeX = (kScreenWidth - timeWidth) / 2;
  tft_.drawString(timeStr, timeX, kTimeY);

  // Seconds: smaller, offset to the right of the time
  char secStr[3];
  snprintf(secStr, sizeof(secStr), "%02d", timeinfo.tm_sec);

  tft_.setTextColor(kColorSeconds, kColorBg);
  tft_.setFreeFont(&FreeSans12pt7b);  // smaller font for seconds
  tft_.drawString(secStr, kSecX, kTimeY + 10);

  // Date and day of week
  char dateStr[32];
  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  snprintf(dateStr, sizeof(dateStr), "%s, %d %s %d",
           days[timeinfo.tm_wday],
           timeinfo.tm_mday,
           months[timeinfo.tm_mon],
           timeinfo.tm_year + 1900);

  tft_.setTextColor(kColorDate, kColorBg);
  tft_.setFreeFont(&FreeSans9pt7b);
  int dateWidth = tft_.textWidth(dateStr);
  tft_.drawString(dateStr, (kScreenWidth - dateWidth) / 2, kDateY);
}

// ── Message Overlay ──────────────────────────────────────────────────────────

void DisplayManager::renderMessageOverlay() {
  tft_.fillScreen(kColorBg);

  tft_.setTextColor(kColorMessage, kColorBg);
  tft_.setFreeFont(&FreeSans12pt7b);

  // Word-wrap simple message
  tft_.setTextWrap(true);
  tft_.drawString(overlayMessage_, 10, kMsgY);

  // Duration countdown
  char countdownStr[16];
  unsigned long remaining = overlayDurationMs_ - (millis() - overlayStartMs_);
  snprintf(countdownStr, sizeof(countdownStr), "%lu s", remaining / 1000);
  tft_.setFreeFont(&FreeSans9pt7b);
  tft_.setTextColor(TFT_LIGHTGREY, kColorBg);
  tft_.drawRightString(countdownStr, kScreenWidth - 10, kScreenHeight - 20, 0);
}

// ── Alarm Ringing ────────────────────────────────────────────────────────────

void DisplayManager::renderAlarmRinging() {
  // Flash the screen background
  static bool toggle = false;
  toggle = !toggle;

  uint16_t flashColor = toggle ? kColorAlarm : kColorBg;
  tft_.fillScreen(flashColor);

  tft_.setTextColor(TFT_WHITE, flashColor);
  tft_.setFreeFont(&FreeSans18pt7b);
  tft_.drawString("ALARM!", 60, 60);

  tft_.setFreeFont(&FreeSans12pt7b);
  tft_.drawString("Dismiss    Snooze", 50, 130);
}

// ── Command Queue Processing ─────────────────────────────────────────────────

void DisplayManager::processCommandQueue() {
  CommandMessage cmd;
  while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
    switch (cmd.type) {
      case CommandMessage::Type::kSetBrightness:
        handleSetBrightness(cmd);
        break;
      case CommandMessage::Type::kDisplayMessage:
        handleDisplayMessage(cmd);
        break;
      case CommandMessage::Type::kSetAlarm:
        handleSetAlarm(cmd);
        break;
    }
  }
}

void DisplayManager::handleSetBrightness(const CommandMessage& cmd) {
  uint8_t level = cmd.brightnessLevel;
  if (level > 100) level = 100;
  brightnessLevel_ = level;
  setBacklight(level);
  persistBrightness(level);
  sendCommandResult("set_brightness", "applied", "");
  sendDisplayState();
}

void DisplayManager::handleDisplayMessage(const CommandMessage& cmd) {
  strncpy(overlayMessage_, cmd.message, sizeof(overlayMessage_) - 1);
  overlayDurationMs_ = (cmd.durationSeconds > 0)
    ? (unsigned long)cmd.durationSeconds * 1000
    : kOverlayDefaultDurationMs;
  overlayStartMs_ = millis();

  previousMode_ = currentMode_;
  currentMode_ = DisplayMode::kMessage;
  renderMessageOverlay();

  sendCommandResult("display_message", "applied", "");
  sendDisplayState();
}

void DisplayManager::handleSetAlarm(const CommandMessage& cmd) {
  // Alarm handling stub — will be implemented in Story 2.3/3.1
  (void)cmd;
  sendCommandResult("set_alarm", "applied", "stored");
}

// ── Event Helpers ────────────────────────────────────────────────────────────

void DisplayManager::sendCommandResult(const char* commandType, const char* status, const char* detail) {
  EventMessage evt = {};
  evt.type = EventMessage::Type::kCommandResult;
  strncpy(evt.commandType, commandType, sizeof(evt.commandType) - 1);
  strncpy(evt.status, status, sizeof(evt.status) - 1);
  strncpy(evt.detail, detail, sizeof(evt.detail) - 1);
  xQueueSend(eventQueue, &evt, 0);
}

void DisplayManager::sendDisplayState() {
  EventMessage evt = {};
  evt.type = EventMessage::Type::kDisplayState;
  evt.brightness = brightnessLevel_;
  const char* modeStr = "clock";
  switch (currentMode_) {
    case DisplayMode::kAlarmRinging: modeStr = "alarm_ringing"; break;
    case DisplayMode::kMessage:      modeStr = "message";       break;
    case DisplayMode::kClock:        modeStr = "clock";         break;
    case DisplayMode::kError:        modeStr = "error";         break;
  }
  strncpy(evt.displayMode, modeStr, sizeof(evt.displayMode) - 1);
  xQueueSend(eventQueue, &evt, 0);
}

void DisplayManager::sendAlarmState(bool armed) {
  EventMessage evt = {};
  evt.type = EventMessage::Type::kAlarmState;
  evt.alarmArmed = armed;
  xQueueSend(eventQueue, &evt, 0);
}

unsigned long DisplayManager::millis() const {
  return ::millis();
}

// ── FreeRTOS Task Entry ──────────────────────────────────────────────────────

void displayTaskEntry(void* pvParameters) {
  DisplayManager mgr;
  mgr.begin();

  for (;;) {
    mgr.loop();
  }
}
