# Hardware interfaces

Target: GUITION JC3636K718C_I_YR1, ESP32-S3, 360 × 360 ST77916 display, CST816S touch, 16 MB flash and 8 MB PSRAM.
The public interfaces are in `src/display/drivers/GuitionKnob/Hardware.h` and `KnobPanel.h`.

| Interface | Configuration | Entry point |
| --- | --- | --- |
| LCD | SPI2 QSPI 80 MHz; CLK11, CS12, D0–D3=13–16, RST17; RGB565 | `KnobPanel::begin()`, `display()` |
| Touch | SDA9, SCL10, RST8; 400 kHz; 0x15, with 0x2a fallback | `readTouch(x, y)` |
| Backlight | GPIO21, LEDC channel 0, 5 kHz/10-bit; levels 0–16 | `setBrightness(level)` |
| Battery/power | GPIO6, SYS/2 divider; USB SOF as positive host evidence | `Hardware::snapshot()` |
| Rotary contacts | GPIO2/A and GPIO1/B; independent direction contacts | `snapshot().encoderDetents` |
| RGB ring | GPIO0, SPI3 DMA, GRB at 2.5 MHz; 12 physical LEDs | `configureLeds()`, `publishLedGauges()` |
| Speaker | I2S1 TX, 44.1 kHz/16-bit stereo; BCK3, WS45, DATA42, EN46 | `requestTest(HardwareTest::Speaker, jobId)` |
| Microphone | I2S0 PDM RX, 16 kHz/16-bit mono; CLK5, DATA4 | `requestTest(HardwareTest::Microphone, jobId)` |

## Integration

The existing startup owns `Hardware::begin()`. Reuse the singleton and its value snapshots; do not start a second driver on the same pins or buses.
Hardware commands are asynchronous. Check `completedJobId`, `lastError` and `result` rather than blocking the UI thread.
Speaker playback is currently a short test tone; microphone sampling returns statistics for about two seconds. Continuous audio needs a separate application-level data path.

Clockwise rotation produces negative raw detents on the tested screen; the UI applies −1 so clockwise increases the selected field. Do not decode these contacts as a quadrature encoder.
The decoder samples every 500 µs, qualifies 6 ms low and 2 ms both-high, and rejects ambiguous or startup-held pulses.

LED 0 is at 12 o'clock, numbering clockwise; LED 6 is at the bottom. UI mode follows the actual visible meter colors, leaves the unfilled arc white and turns the top/bottom gaps off.
`LedConfig` supports off, UI, identify and fixed-color modes. Brightness is 0–255; mode/brightness are RAM-only (boot defaults: off/32).

These interfaces have been exercised on the target board. Reuse their known configuration; new features need checks for their own behavior, not repeated pin discovery.
Battery capacity is not calibrated, charge-only adapters can be ambiguous, audio quality is not qualified, and optional haptics are not validated.

## HTTP interfaces

- `GET /api/hardware`: hardware snapshot.
- `POST /api/hardware/test`: form `action=led|speaker|mic`; returns an accepted job ID.
- `POST /api/hardware/led`: form `mode`, `top`, `clockwise`, `brightness`; optional `count` and `white` (six hex digits). Use `count=12`, `top=0`, `clockwise=1` for this board.

Commands return 202 when accepted, 409 when busy/unavailable, and 400 for invalid parameters. They are intended for a trusted local network. The former `/hardware` page returns 404.
