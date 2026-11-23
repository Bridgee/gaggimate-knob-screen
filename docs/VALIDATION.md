# Validation and upstream baseline

## What is being validated

The current port targets **official v1.10.0**, commit `34bf9fc5f6cf89c9929636fc23092b75284092f9`, controller protocol 16. The controller image in matched packages is the hash-pinned, unmodified official release. A successful build is software evidence; it does not certify a particular device or binary on hardware.

| Evidence | What it supports | What it does not establish |
| --- | --- | --- |
| Historical 0.3.8 and 0.4.0-rc.8 hardware acceptance | Known panel, touch, brightness, rotary contacts, LED layout, standby and peripheral configuration | Acceptance of the new protocol-16 application |
| Source comparison with the accepted port and official releases | Identifies retained drivers and the integrations requiring new checks | Timing, electrical operation or stored-data migration on a running board |
| Official control-source parity check | Controller, protocol, PID and process files retain the selected release's behavior, apart from one removed filepath comment | Fixes or features added to upstream after that release |
| Native LVGL/EEZ tests | Actual generated pages, both themes, original handlers/flow integrity, rotary targets, standby content and status-row layout | Physical LCD output, real touch latency or coffee-control execution; most application actions are stubbed |
| Firmware-generated nanopb and queue tests | Actual schema/runtime encoding, indexed telemetry/commands, malformed-input bounds and coalescing/priority rules | BLE connection, retransmission, RTOS scheduling or controller responses |
| Full build, exact source replay and artifact checks | Source provenance, locked inputs, valid packages and reproducibility for the environments actually compared | Physical installation, flash/NVS migration or brewing qualification |

The native status-row test exercises all eight Wi-Fi/Bluetooth/update visibility combinations with the battery icon, on menu, standby and welcome screens, in both themes. It checks visibility, spacing, row bounds, the round display boundary and rendered pixels. The protocol test runs after firmware generation and compiles the same generated nanopb source and pinned runtime; it never actuates equipment.

The K718 board definition, panel initialization, rotary decoder, LED mapping/encoding, battery artwork, parameter policies and standby renderer retain their accepted implementations. Startup health callbacks and the new upstream UI integration are changes. The battery stability filter was introduced after the last physical acceptance: its software cases pass, but its current battery-only behavior remains unverified on hardware. Voltage-based capacity remains uncalibrated.

K718 startup allocation, Wi-Fi buffer policy and HTTP image-response ownership/admission also changed during this migration; see [Hardware interfaces](HARDWARE.md#memory-allocation). Reuse prior pin and peripheral configuration, but include these shared runtime inputs when selecting current-binary regressions. HTTP concurrency, interrupted reads and restart persistence need their own evidence; they cannot qualify BLE timing or physical input behavior.

## Validation without changing an installed machine

1. Compare the previous official release, physically accepted port, new official release and candidate source. Record unchanged modules and changed integrations separately.
2. Run the publication check, control-source parity, native UI/policy tests, generated-protocol checks, web tests, full build and exact official-base reconstruction. Verify the matched controller asset and package hashes.
3. Keep raw evidence and device records outside the public repository. Tie every result to its source tree, CI run and application hash. Preserve earlier results without relabelling them as new acceptance.
4. When hardware becomes available, a screen-only USB smoke test can cover local display/touch/knob, standby, power and web behavior without changing a controller's pairing. It cannot validate end-to-end controller communication.
5. A stable protocol-16 release still needs focused acceptance of the exact final binary with a compatible controller: handshake/reconnection, ambient telemetry, settings/profile retention and changed UI behavior. Follow the [single-controller workflow](TESTING.md). Defer real-controller acceptance until an intentional changeover; do not require a spare board or disturb an installed pairing merely to complete the checklist. Keep a software-verified candidate labelled as such until the missing checks are done.

Real brewing dynamics remain outside the USB acceptance scope. OTA qualification is separate; all normal installers and the current migration candidate's test-install permission remain disabled. Historical protocol-5 OTA tests do not qualify protocol 16.

## Why GitHub can report this fork as behind

GitHub compares this branch with upstream **master**, whereas the port deliberately pins a reviewed **release tag**. Commits after that tag are additional upstream development, not omitted changes from the selected release. Ahead/behind counts alone do not measure adaptation completeness. Check the merge base, the pinned commit in `port/config.json`, source parity and migration tests.

At the reviewed master snapshot `26ee775eb94d043717c7b529e91eeb8c10e79b94`, there are ten commits after v1.10.0:

| Commit | Change | Treatment |
| --- | --- | --- |
| `24b0104b` | Allow Dependabot in the CLA bot | Upstream project tooling |
| `818ca889` | Swap Max expansion-header UART pins | Controller hardware fix; review with the next matched official controller |
| `175f5cc8` | Vancouver stays on UTC-7 | Time-zone data fix |
| `f61198fa` | Linux simulator build | Simulator tooling |
| `94306231` | Show the web 404 page for unknown paths | Web routing fix |
| `bb4805e8` | Save profiles through a temporary file and rename | Relevant profile-storage robustness fix to review for the next update |
| `10d6ae21` | Stamp manually stopped process end times | Relevant elapsed-time reporting fix to review for the next update |
| `3b22f0c3` | Correct SimplePID sampling-frequency gating | Controller-side fix; upstream notes no change at the heaters' 1 Hz rate |
| `3bf85f8d` | Optional scheduled switch-off time | New feature touching settings, web UI and wake-up behavior |
| `26ee775e` | Accept String paths in simulator rename | Simulator support for the profile-save change |

These changes are **not included** in the current v1.10.0-based port. They are reviewed follow-up inputs, not prerequisites for claiming that release's source was adapted. Do not merge a moving master or silently cherry-pick controller changes while distributing an unchanged official controller image. A deliberate backport needs its own source identity, compatibility review and regression checks; a new official release follows [the migration workflow](MAINTAINING.md#upstream-updates).
