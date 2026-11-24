# 0.5.0-rc.13 — replay device fixes

Official base: **GaggiMate v1.10.0 / protocol 16**. Normal screen/controller/combined OTA permissions remain disabled.

The first rc.12 hardware run exposed two validation-only integration defects. The synthetic `simulated` identity must bypass the unchanged legacy physical-controller OTA parser. SystemInfo must also mark its capability submessage present; otherwise pressure/dimming/scale capabilities disappear during encoding.

This candidate fixes both and adds host regressions that compile the actual ready callback with the real legacy parser in both roles, and inspect capabilities after real nanopb encoding/decoding. It retains the isolated replay transport, settings/profiles/images, explicit screen-only OTA qualification and checked USB recovery.

Physical acceptance must be recorded for this exact image. Startup, replay/interaction, actual download/install, interruption and USB recovery are separate results. No real BLE, control-board OTA or brewing qualification is implied. See [Testing](TESTING.md) and [Building and flashing](BUILDING.md).
