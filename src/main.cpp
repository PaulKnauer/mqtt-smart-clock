#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "board_config.h"

namespace {

TFT_eSPI tft = TFT_eSPI();

void setRgbLed(bool redOn, bool greenOn, bool blueOn) {
  // The onboard RGB LED is common-anode, so LOW turns a color on.
  digitalWrite(board::kLedRed, redOn ? LOW : HIGH);
  digitalWrite(board::kLedGreen, greenOn ? LOW : HIGH);
  digitalWrite(board::kLedBlue, blueOn ? LOW : HIGH);
}

void drawUi() {
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(1);

  tft.fillRoundRect(8, 8, 304, 224, 14, TFT_NAVY);
  tft.drawRoundRect(8, 8, 304, 224, 14, TFT_CYAN);

  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("mqtt-smart-clock", 20, 20, 4);

  tft.setTextColor(TFT_YELLOW, TFT_NAVY);
  tft.drawString("Hosyond 2.8\" ESP32", 20, 58, 2);

  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("Display driver: ILI9341", 20, 88, 2);
  tft.drawString("Touch IC: XPT2046 (separate SPI)", 20, 108, 2);
  tft.drawString("MicroSD SPI: GPIO5/18/19/23", 20, 128, 2);
  tft.drawString("USB flashing: ready", 20, 148, 2);

  tft.setTextColor(TFT_GREENYELLOW, TFT_NAVY);
  tft.drawString("Next step:", 20, 184, 2);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("pio run -t upload", 20, 208, 2);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

  pinMode(board::kLedRed, OUTPUT);
  pinMode(board::kLedGreen, OUTPUT);
  pinMode(board::kLedBlue, OUTPUT);
  pinMode(board::kAudioEnable, OUTPUT);
  pinMode(board::kBootButton, INPUT_PULLUP);
  pinMode(board::kTouchIrq, INPUT);
  pinMode(board::kBatteryAdc, INPUT);

  setRgbLed(false, true, false);
  // Keep the speaker amplifier muted during bring-up. LOW would enable it.
  digitalWrite(board::kAudioEnable, HIGH);

  tft.init();
  drawUi();

  Serial.println();
  Serial.println("Hosyond 2.8 inch ESP32 display firmware booted.");
  Serial.printf("CPU frequency: %u MHz\n", getCpuFrequencyMhz());
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.println("Board resources:");
  Serial.printf("  LCD SPI  : CS=%u DC=%u SCK=%u MOSI=%u MISO=%u BL=%u\n",
                board::kTftCs, board::kTftDc, board::kTftSclk, board::kTftMosi,
                board::kTftMiso, board::kTftBacklight);
  Serial.printf("  Touch SPI: CS=%u IRQ=%u SCK=%u MOSI=%u MISO=%u\n",
                board::kTouchCs, board::kTouchIrq, board::kTouchSclk,
                board::kTouchMosi, board::kTouchMiso);
  Serial.printf("  SD SPI   : CS=%u SCK=%u MOSI=%u MISO=%u\n",
                board::kSdCs, board::kSdSclk, board::kSdMosi, board::kSdMiso);
  Serial.printf("  Audio    : EN=%u DAC=%u (active-low enable)\n",
                board::kAudioEnable, board::kAudioDac);
}

void loop() {
  static uint32_t lastToggleMs = 0;
  static bool ledState = false;

  const uint32_t now = millis();
  if (now - lastToggleMs >= 500) {
    lastToggleMs = now;
    ledState = !ledState;
    setRgbLed(false, ledState, false);
  }
}
