#include "KnobPanel.h"
#include "LcdInit.h"
#include "Diagnostics.h"
#include <Wire.h>

namespace guition_knob {
void KnobLcd::tftInit() {
    pinMode(LcdReset, OUTPUT);
    digitalWrite(LcdReset, HIGH);
    delay(10);
    digitalWrite(LcdReset, LOW);
    delay(120);
    digitalWrite(LcdReset, HIGH);
    delay(120);
    // GUITION's esp_lcd driver sends COLMOD before the vendor table. The
    // table itself does not select a pixel format; explicitly match RGB565.
    qspi->beginWrite();
    qspi->writeC8D8(ST77916_COLMOD, 0x55);
    qspi->endWrite();
    // The generic ST77916 table in Arduino_GFX targets a different panel.
    // Send GUITION's panel-specific table, preserving its command delays.
    for (const auto &entry : InitCommands) {
        qspi->beginWrite();
        qspi->writeC8Bytes(entry.command, const_cast<uint8_t *>(entry.data), entry.length);
        qspi->endWrite();
        if (entry.delayMs)
            delay(entry.delayMs);
    }
    delay(20);
}

bool KnobPanel::begin() {
    pinMode(Backlight, OUTPUT);
    digitalWrite(Backlight, LOW);
    ledcSetup(0, 5000, 10);
    ledcAttachPin(Backlight, 0);
    setBrightness(0);
    if (!psramFound()) {
        knobDiagnosticPrintf("K718: PSRAM missing; check 8 MB OPI PSRAM configuration\n");
        return false;
    }
    if (!lcd.begin(80000000)) {
        knobDiagnosticPrintf("K718: QSPI initialization failed\n");
        return false;
    }
    lcd.fillScreen(BLACK);

    pinMode(TouchReset, OUTPUT);
    digitalWrite(TouchReset, LOW);
    delay(200);
    digitalWrite(TouchReset, HIGH);
    delay(200);
    if (Wire.begin(TouchSda, TouchScl, 400000)) {
        Wire.setTimeOut(10);
        for (const auto address : {TouchAddress, AlternateTouchAddress}) {
            Wire.beginTransmission(address);
            if (Wire.endTransmission() == 0) {
                touchAddress = address;
                touchReady = true;
                break;
            }
        }
    }
    knobDiagnosticPrintf("K718: LCD 360x360, PSRAM %u bytes, touch 0x%02x %s\n",
                  ESP.getPsramSize(), touchAddress, touchReady ? "present" : "not detected");
    return true;
}

bool KnobPanel::readTouch(uint16_t &x, uint16_t &y) {
    // Poll without depending on the interrupt wiring, which differs between
    // GUITION's factory demo (GPIO7) and its newer board registry (not used).
    // Read failures are releases, never a stale held press.
    if (!touchReady)
        return false;
    Wire.beginTransmission(touchAddress);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0)
        return false;
    if (Wire.requestFrom(touchAddress, static_cast<uint8_t>(5)) != 5)
        return false;
    uint8_t data[5];
    for (auto &byte : data)
        byte = Wire.read();
    touchReady = true;
    return decodeTouch(data, x, y);
}

void KnobPanel::setBrightness(int level) {
    const uint16_t duty = backlightDuty(level);
    ledcWrite(0, duty);
    knobDiagnosticPrintf("K718 backlight request: level=%d duty=%u/1023\n", level, duty);
}
} // namespace guition_knob
