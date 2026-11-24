# 0.5.0-rc.14 — Brew rotary Profile shortcut

Based on official **GaggiMate v1.10.0 / protocol 16**.

Turn the knob on the idle Brew main view to open the existing Profile picker and preview in that direction. Further detents browse one profile at a time, stopping at either end. Only the existing checkmark loads the preview; there is no additional highlight. The original arrows and parameter-panel adjustment remain available.

The shortcut checks current screen/mode, pending transition, parameter panel, confirmation overlay, loaded profiles, controller readiness/connection/protocol and active/update/error state. It never starts a process. Disabled motion is discarded. The host suite executes the actual guard/preview methods, and the native LVGL suite checks decoder routing and preserved parameter controls in both themes.

Build and Knob-only acceptance must be recorded for this exact candidate. The validation target has isolated local replay and storage; it does not connect to the daily-use controller. No new real BLE, controller OTA or brewing acceptance is implied. All ordinary screen/controller/combined/legacy OTA permissions remain disabled. See [Testing](TESTING.md) and [Building and flashing](BUILDING.md).
