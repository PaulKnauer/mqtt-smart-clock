#pragma once

#include <Arduino.h>

namespace board {

constexpr uint8_t kTftMiso = 12;
constexpr uint8_t kTftMosi = 13;
constexpr uint8_t kTftSclk = 14;
constexpr uint8_t kTftCs = 15;
constexpr uint8_t kTftDc = 2;
constexpr uint8_t kTftBacklight = 21;

constexpr uint8_t kTouchSclk = 25;
constexpr uint8_t kTouchMosi = 32;
constexpr uint8_t kTouchMiso = 39;
constexpr uint8_t kTouchCs = 33;
constexpr uint8_t kTouchIrq = 36;

constexpr uint8_t kSdCs = 5;
constexpr uint8_t kSdMosi = 23;
constexpr uint8_t kSdSclk = 18;
constexpr uint8_t kSdMiso = 19;

constexpr uint8_t kAudioEnable = 4;
constexpr uint8_t kAudioDac = 26;

constexpr uint8_t kLedRed = 22;
constexpr uint8_t kLedGreen = 16;
constexpr uint8_t kLedBlue = 17;

constexpr uint8_t kBootButton = 0;
constexpr uint8_t kBatteryAdc = 34;
constexpr uint8_t kExpansionInput = 35;

constexpr bool kLedActiveLow = true;
constexpr bool kAudioEnableActiveLow = true;
constexpr uint8_t kBacklightOnLevel = HIGH;

}  // namespace board
