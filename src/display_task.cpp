#include "display_task.h"
#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <time.h>
#include "board_config.h"
#include "device_config.h"
#include "queue_handles.h"

DisplayManager* DisplayManager::instance = nullptr;

constexpr int kScreenWidth  = 320;
constexpr int kScreenHeight = 240;

constexpr int kTimeY   = 80;
constexpr int kSecX    = 220;
constexpr int kDateY   = 140;
constexpr int kMsgY    = 100;

constexpr uint16_t kColorBg       = TFT_BLACK;
constexpr uint16_t kColorTime     = TFT_WHITE;
constexpr uint16_t kColorSeconds  = TFT_SKYBLUE;
constexpr uint16_t kColorDate     = TFT_LIGHTGREY;
constexpr uint16_t kColorMessage  = TFT_YELLOW;
constexpr uint16_t kColorAlarm    = TFT_RED;

constexpr int kPwmChannel    = 0;
constexpr int kPwmFreq       = 5000;
constexpr int kPwmResolution = 8;

constexpr char kNvsNamespace[]         = "clock";
constexpr char kNvsBrightnessKey[]     = "brightness";
constexpr char kNvsAlarmTimeKey[]      = "alarm_time";
constexpr char kNvsAlarmLabelKey[]     = "alarm_label";

constexpr unsigned long kOverlayDefaultDurationMs = 10000;
constexpr unsigned long kAlarmAutoStopMs          = 60000;
constexpr unsigned long kHeartbeatIntervalMs      = 30000;

// ── Construction ─────────────────────────────────────────────────────────────

DisplayManager::DisplayManager()
  : currentMode_(DisplayMode::kClock),
    previousMode_(DisplayMode::kClock),
    overlayStartMs_(0),
    overlayDurationMs_(0),
    brightnessLevel_(device_config::kDefaultBrightness),
    lastMinute_(-1),
    storedAlarmTime_(0),
    alarmStartMs_(0),
    alarmRinging_(false) {
  instance = this;
  memset(overlayMessage_, 0, sizeof(overlayMessage_));
  memset(storedAlarmLabel_, 0, sizeof(storedAlarmLabel_));
}

void DisplayManager::begin() {
  initNvs();

  tft_.init();
  tft_.setRotation(1);
  tft_.fillScreen(kColorBg);

  setupBacklight();
  brightnessLevel_ = loadBrightness();
  setBacklight(brightnessLevel_);

  // Load stored alarm
  loadAlarm();

  // Init touch (CS=33, IRQ=36)
  touch_ = XPT2046_Touchscreen(board::kTouchCs, board::kTouchIrq);
  touch_.begin();
}

void DisplayManager::loop() {
  processCommandQueue();

  time_t now = time(nullptr);

  // Evaluate alarm
  evaluateAlarm(now);

  // Auto-stop alarm after timeout
  if (alarmRinging_ && (millis() - alarmStartMs_ >= kAlarmAutoStopMs)) {
    stopAlarm("timeout");
  }

  if (currentMode_ == DisplayMode::kClock) {
    renderClockFace();
    vTaskDelay(pdMS_TO_TICKS(500));
  } else if (currentMode_ == DisplayMode::kMessage) {
    if (millis() - overlayStartMs_ >= overlayDurationMs_) {
      currentMode_ = DisplayMode::kClock;
      tft_.fillScreen(kColorBg);
      lastMinute_ = -1;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  } else if (currentMode_ == DisplayMode::kAlarmRinging) {
    renderAlarmRinging();

    // Poll touch for dismiss/snooze
    if (touch_.tirqTouched() || touch_.touched()) {
      TS_Point p = touch_.getPoint();
      // Map touch coordinates to screen (XPT2046 returns 0-4095)
      // Screen is 320x240 landscape. Touch maps: x ~ y, y ~ x
      int tx = map(p.x, 200, 3800, 0, kScreenWidth);
      int ty = map(p.y, 200, 3800, 0, kScreenHeight);
      // Constrain
      if (tx < 0) tx = 0; if (tx > kScreenWidth) tx = kScreenWidth;
      if (ty < 0) ty = 0; if (ty > kScreenHeight) ty = kScreenHeight;

      // Button regions: [Dismiss] left half, [Snooze] right half, y=130-170
      if (ty >= 130 && ty <= 170) {
        if (tx < kScreenWidth / 2) {
          stopAlarm("dismiss");
        } else {
          // Snooze: re-arm for 5 minutes
          time_t snoozeTime = time(nullptr) + 300; // +5 minutes
          saveAlarm(snoozeTime, storedAlarmLabel_);
          stopAlarm("snooze");
        }
      }
      delay(200); // debounce
      while (touch_.touched()) { delay(50); } // wait for release
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  // Periodic heartbeat
  static unsigned long lastHb = 0;
  if (millis() - lastHb >= kHeartbeatIntervalMs) {
    lastHb = millis();
    EventMessage evt = {};
    evt.type = EventMessage::Type::kCommandResult; // reuse, not ideal
    xQueueSend(eventQueue, &evt, 0);
  }
}

// ── Backlight ────────────────────────────────────────────────────────────────

void DisplayManager::setupBacklight() {
  ledcSetup(kPwmChannel, kPwmFreq, kPwmResolution);
  ledcAttachPin(board::kTftBacklight, kPwmChannel);
}

void DisplayManager::setBacklight(uint8_t level) {
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

void DisplayManager::initNvs() {
  Preferences prefs;
  prefs.begin(kNvsNamespace, false);
  prefs.end();
}

// ── Alarm Persistence ────────────────────────────────────────────────────────

void DisplayManager::saveAlarm(time_t alarmTime, const char* label) {
  storedAlarmTime_ = alarmTime;
  strncpy(storedAlarmLabel_, label, sizeof(storedAlarmLabel_) - 1);
  Preferences prefs;
  prefs.begin(kNvsNamespace, false);
  prefs.putLong(kNvsAlarmTimeKey, (long)alarmTime);
  prefs.putString(kNvsAlarmLabelKey, label);
  prefs.end();
  sendAlarmState(true);
}

void DisplayManager::clearAlarm() {
  storedAlarmTime_ = 0;
  storedAlarmLabel_[0] = '\0';
  Preferences prefs;
  prefs.begin(kNvsNamespace, false);
  prefs.remove(kNvsAlarmTimeKey);
  prefs.remove(kNvsAlarmLabelKey);
  prefs.end();
  sendAlarmState(false);
}

void DisplayManager::loadAlarm() {
  Preferences prefs;
  prefs.begin(kNvsNamespace, true);
  storedAlarmTime_ = (time_t)prefs.getLong(kNvsAlarmTimeKey, 0);
  String label = prefs.getString(kNvsAlarmLabelKey, "");
  strncpy(storedAlarmLabel_, label.c_str(), sizeof(storedAlarmLabel_) - 1);
  prefs.end();
}

void DisplayManager::evaluateAlarm(time_t now) {
  if (alarmRinging_) return;               // already ringing
  if (storedAlarmTime_ == 0) return;       // no alarm set
  if (now < storedAlarmTime_) return;      // not yet

  // Time to ring!
  currentMode_ = DisplayMode::kAlarmRinging;
  alarmRinging_ = true;
  alarmStartMs_ = millis();
  tft_.fillScreen(kColorBg);

  // Publish alarm_triggered event
  EventMessage evt = {};
  evt.type = EventMessage::Type::kAlarmTriggered;
  evt.alarmTimeUtc = storedAlarmTime_;
  strncpy(evt.alarmLabel, storedAlarmLabel_, sizeof(evt.alarmLabel) - 1);
  xQueueSend(eventQueue, &evt, 0);

  // Audio: tone on DAC
  pinMode(board::kAudioEnable, OUTPUT);
  digitalWrite(board::kAudioEnable, HIGH);  // active low, LOW = enable
  // Use ledc on audio pin for tone
  ledcSetup(1, 2000, 8);
  ledcAttachPin(board::kAudioDac, 1);
  ledcWriteTone(1, 2000);
}

void DisplayManager::stopAlarm(const char* action) {
  // Stop tone
  ledcDetachPin(board::kAudioDac);
  pinMode(board::kAudioDac, OUTPUT);
  digitalWrite(board::kAudioDac, LOW);
  digitalWrite(board::kAudioEnable, LOW);  // disable amp

  alarmRinging_ = false;
  clearAlarm();

  // Publish alarm_acknowledged event
  EventMessage evt = {};
  evt.type = EventMessage::Type::kAlarmAcknowledged;
  strncpy(evt.action, action, sizeof(evt.action) - 1);
  strncpy(evt.source, "timer", sizeof(evt.source) - 1);
  xQueueSend(eventQueue, &evt, 0);

  currentMode_ = DisplayMode::kClock;
  tft_.fillScreen(kColorBg);
  lastMinute_ = -1;
}

// ── Clock Face ───────────────────────────────────────────────────────────────

void DisplayManager::renderClockFace() {
  if (currentMode_ != DisplayMode::kClock) return;

  time_t now = time(nullptr);
  struct tm timeinfo;
  if (!localtime_r(&now, &timeinfo)) return;

  int currentMinute = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  if (currentMinute == lastMinute_) return;
  lastMinute_ = currentMinute;

  tft_.fillScreen(kColorBg);

  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  tft_.setTextColor(kColorTime, kColorBg);
  tft_.setFreeFont(&FreeSansBold24pt7b);
  int timeWidth = tft_.textWidth(timeStr);
  tft_.drawString(timeStr, (kScreenWidth - timeWidth) / 2, kTimeY);

  char secStr[3];
  snprintf(secStr, sizeof(secStr), "%02d", timeinfo.tm_sec);
  tft_.setTextColor(kColorSeconds, kColorBg);
  tft_.setFreeFont(&FreeSans12pt7b);
  tft_.drawString(secStr, kSecX, kTimeY + 10);

  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char dateStr[32];
  snprintf(dateStr, sizeof(dateStr), "%s, %d %s %d",
           days[timeinfo.tm_wday], timeinfo.tm_mday,
           months[timeinfo.tm_mon], timeinfo.tm_year + 1900);
  tft_.setTextColor(kColorDate, kColorBg);
  tft_.setFreeFont(&FreeSans9pt7b);
  tft_.drawString(dateStr, (kScreenWidth - tft_.textWidth(dateStr)) / 2, kDateY);
}

// ── Message Overlay ──────────────────────────────────────────────────────────

void DisplayManager::showMessage(const char* msg, unsigned long durationMs) {
  strncpy(overlayMessage_, msg, sizeof(overlayMessage_) - 1);
  overlayDurationMs_ = durationMs;
  overlayStartMs_ = millis();
  previousMode_ = currentMode_;
  currentMode_ = DisplayMode::kMessage;

  tft_.fillScreen(kColorBg);
  tft_.setTextColor(kColorMessage, kColorBg);
  tft_.setFreeFont(&FreeSans12pt7b);
  tft_.setTextWrap(true);
  tft_.drawString(overlayMessage_, 10, kMsgY);
}

// ── Alarm Ringing ────────────────────────────────────────────────────────────

void DisplayManager::renderAlarmRinging() {
  static bool toggle = false;
  toggle = !toggle;
  uint16_t flashColor = toggle ? kColorAlarm : kColorBg;
  tft_.fillScreen(flashColor);
  tft_.setTextColor(TFT_WHITE, flashColor);
  tft_.setFreeFont(&FreeSans18pt7b);
  tft_.drawString("ALARM!", 60, 60);
  if (strlen(storedAlarmLabel_) > 0) {
    tft_.setFreeFont(&FreeSans9pt7b);
    tft_.drawString(storedAlarmLabel_, 60, 100);
  }
  tft_.setFreeFont(&FreeSans12pt7b);
  tft_.drawString("[Dismiss]  [Snooze]", 40, 150);
}

// ── Command Queue ────────────────────────────────────────────────────────────

void DisplayManager::processCommandQueue() {
  CommandMessage cmd;
  while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
    switch (cmd.type) {
      case CommandMessage::Type::kSetBrightness: {
        uint8_t level = cmd.brightnessLevel;
        if (level > 100) level = 100;
        brightnessLevel_ = level;
        setBacklight(level);
        persistBrightness(level);
        sendCommandResult("set_brightness", "applied", "");
        sendDisplayState();
        break;
      }
      case CommandMessage::Type::kDisplayMessage: {
        unsigned long dur = (cmd.durationSeconds > 0)
          ? (unsigned long)cmd.durationSeconds * 1000 : kOverlayDefaultDurationMs;
        showMessage(cmd.message, dur);
        sendCommandResult("display_message", "applied", "");
        sendDisplayState();
        break;
      }
      case CommandMessage::Type::kSetAlarm: {
        if (cmd.alarmTimeUtc > 0) {
          saveAlarm(cmd.alarmTimeUtc, cmd.alarmLabel);
          sendCommandResult("set_alarm", "applied", "stored");
        } else {
          sendCommandResult("set_alarm", "rejected", "invalid time");
        }
        break;
      }
    }
  }
}

// ── Event Helpers ────────────────────────────────────────────────────────────

void DisplayManager::sendCommandResult(const char* type, const char* status, const char* detail) {
  EventMessage evt = {};
  evt.type = EventMessage::Type::kCommandResult;
  strncpy(evt.commandType, type, sizeof(evt.commandType) - 1);
  strncpy(evt.status, status, sizeof(evt.status) - 1);
  strncpy(evt.detail, detail, sizeof(evt.detail) - 1);
  xQueueSend(eventQueue, &evt, 0);
}

void DisplayManager::sendDisplayState() {
  EventMessage evt = {};
  evt.type = EventMessage::Type::kDisplayState;
  evt.brightness = brightnessLevel_;
  const char* m = "clock";
  switch (currentMode_) {
    case DisplayMode::kAlarmRinging: m = "alarm_ringing"; break;
    case DisplayMode::kMessage:      m = "message";       break;
    case DisplayMode::kClock:        m = "clock";         break;
    case DisplayMode::kError:        m = "error";         break;
  }
  strncpy(evt.displayMode, m, sizeof(evt.displayMode) - 1);
  xQueueSend(eventQueue, &evt, 0);
}

void DisplayManager::sendAlarmState(bool armed) {
  EventMessage evt = {};
  evt.type = EventMessage::Type::kAlarmState;
  evt.alarmArmed = armed;
  evt.alarmTimeUtc = storedAlarmTime_;
  strncpy(evt.alarmLabel, storedAlarmLabel_, sizeof(evt.alarmLabel) - 1);
  xQueueSend(eventQueue, &evt, 0);
}

unsigned long DisplayManager::millis() const { return ::millis(); }

// ── FreeRTOS Task Entry ──────────────────────────────────────────────────────

void displayTaskEntry(void* pvParameters) {
  DisplayManager mgr;
  mgr.begin();
  for (;;) mgr.loop();
}
