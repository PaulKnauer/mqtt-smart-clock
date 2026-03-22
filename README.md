# mqtt-smart-clock

PlatformIO firmware project for the Hosyond 2.8 inch ESP32 display module based on the ESP32-32E, ILI9341V LCD, and XPT2046 resistive touch controller.

This setup targets the board documented by Hosyond/LCDWiki as the `2.8inch ESP32-32E Display`:

- LCD controller: `ILI9341`
- Resolution: `240x320`
- Board: `ESP32-32E`
- USB programming: `USB-C`
- PlatformIO board profile: `esp32dev`

## Project layout

- `platformio.ini`: PlatformIO environment and TFT pin mapping
- `include/board_config.h`: Full board pin map from the LCDWiki specification
- `src/main.cpp`: Minimal firmware that powers the display, enables the backlight, and renders a boot screen

## Flash the board

1. Connect the display over USB-C.
2. Find the serial port:
   - macOS: `ls /dev/cu.*`
3. Build:

```bash
pio run
```

4. Upload:

```bash
pio run -t upload --upload-port /dev/cu.usbserial-XXXX
```

5. Open the serial monitor:

```bash
pio device monitor --baud 115200 --port /dev/cu.usbserial-XXXX
```

If auto-download does not trigger on your board, hold `BOOT`, tap `RESET`, release `RESET`, then release `BOOT`, and run upload again.

## Board resources

These values come from the LCDWiki page for the `2.8inch ESP32-32E Display`.

### LCD

- `TFT_CS = GPIO15`
- `TFT_DC = GPIO2`
- `TFT_SCLK = GPIO14`
- `TFT_MOSI = GPIO13`
- `TFT_MISO = GPIO12`
- `TFT_RST = EN` shared with ESP32 reset
- `TFT_BL = GPIO21`

### Resistive Touch

- `TOUCH_SCLK = GPIO25`
- `TOUCH_MOSI = GPIO32`
- `TOUCH_MISO = GPIO39`
- `TOUCH_CS = GPIO33`
- `TOUCH_IRQ = GPIO36`
- Controller: `XPT2046`

### MicroSD

- `SD_CS = GPIO5`
- `SD_SCLK = GPIO18`
- `SD_MISO = GPIO19`
- `SD_MOSI = GPIO23`

### Other onboard functions

- `LED_R = GPIO22`
- `LED_G = GPIO16`
- `LED_B = GPIO17`
- `AUDIO_EN = GPIO4` active low
- `AUDIO_DAC = GPIO26`
- `BOOT_KEY = GPIO0`
- `BATTERY_ADC = GPIO34`
- `EXPANSION_INPUT = GPIO35`

## Notes

- The included firmware is a hardware-validation baseline, not the final MQTT clock app yet.
- Touch and MicroSD are now documented and pinned, but not yet initialized in firmware.
- If your exact Hosyond board is the newer `2.8 inch ESP32-S3 Display`, this configuration is not correct and should be split into a second PlatformIO environment.
