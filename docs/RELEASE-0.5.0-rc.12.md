# 0.5.0-rc.12 — controller-free validation candidate

Official base: **GaggiMate v1.10.0 / protocol 16**. The normal K718 image retains the physical-controller workflow and all ordinary OTA permissions remain disabled.

A separately identified `display-knob-validation` image adds synthetic idle/warmup/shot replay using the actual protocol and display callbacks, captured outbound commands, explicit simulation labels and local settings/profile isolation. It does not initialize the machine link, touch its bond store or start coffee-control processes.

The validation image also exposes isolated screen-only OTA qualification after replay is stopped, with local health/standby/slot gates and the shared verified downloader/readback/trial-boot path. It does not assert controller compatibility. The selected own-release image must exist with valid hashes before a real network test; draft releases are ignored.

Normal release/pair packaging rejects validation images. Validation packages require explicit test-image USB handling. See [Testing](TESTING.md) and [Building](BUILDING.md).

This candidate requires physical Knob/recovery acceptance. Host tests and compilation do not qualify BLE, current-protocol connected operation or brewing; real brewing remains outside the USB scope. No control-board OTA, ordinary screen OTA permission or new physical acceptance is implied.
