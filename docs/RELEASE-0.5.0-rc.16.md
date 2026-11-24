# 0.5.0-rc.16 candidate

Based on official GaggiMate v1.10.0, controller protocol 16.

- K718 setup/fallback Wi-Fi is named `gaggimate_knob`; its password and AP address are unchanged.
- Early Wi-Fi events use the existing nonblocking diagnostic output. Saved home-network credentials and connection policy are unchanged.
- The validation-only simulation banner is now created before the UI task starts. Previously controller setup could create LVGL objects concurrently with screen animation/rendering, causing intermittent startup crashes.
- Brew Profile preview, rotary parameter adjustment, LED gauges and standby extensions are retained.

Normal and validation images are separate. The validation image never connects to a physical controller. Normal/controller/combined OTA installation remains disabled. A Wi-Fi name change does not itself repair station authentication; hardware checks and release publication are separate from compilation.
