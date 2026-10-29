#pragma once

#include <Arduino_GFX_Library.h>
#include "Geometry.h"

namespace guition_knob {
// GUITION's jc3636k718 board_peripherals.yaml / board_devices.yaml.
constexpr int LcdClock = 11;
constexpr int LcdCs = 12;
constexpr int LcdD0 = 13;
constexpr int LcdD1 = 14;
constexpr int LcdD2 = 15;
constexpr int LcdD3 = 16;
constexpr int LcdReset = 17;
constexpr int Backlight = 21;
constexpr int TouchReset = 8;
constexpr int TouchSda = 9;
constexpr int TouchScl = 10;
constexpr uint8_t TouchAddress = 0x15; // Factory demo's CST816S driver (7-bit).
constexpr uint8_t AlternateTouchAddress = 0x2a; // Newer GUITION board registry.

class KnobLcd : public Arduino_ST77916 {
  public:
    explicit KnobLcd(Arduino_ESP32QSPI *bus) : Arduino_ST77916(bus, LcdReset, 0, true), qspi(bus) {}

  protected:
    void tftInit() override;

  private:
    Arduino_ESP32QSPI *qspi;
};

class KnobPanel {
  public:
    KnobPanel() : bus(LcdCs, LcdClock, LcdD0, LcdD1, LcdD2, LcdD3), lcd(&bus) {}
    bool begin();
    bool readTouch(uint16_t &x, uint16_t &y);
    void setBrightness(int level);
    bool hasTouch() const { return touchReady; }
    uint8_t touchI2cAddress() const { return touchAddress; }
    KnobLcd &display() { return lcd; }

  private:
    Arduino_ESP32QSPI bus;
    KnobLcd lcd;
    bool touchReady = false;
    uint8_t touchAddress = TouchAddress;
};
} // namespace guition_knob
