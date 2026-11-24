# 0.5.0-rc.15 — distinct Knob hotspot identity

Based on official **GaggiMate v1.10.0 / protocol 16**.

The K718 setup/fallback hotspot is named **gaggimate_knob**, and the native information page reads the same configured name. The saved hotspot password and **http://4.4.4.1** address are retained. Other display targets retain their upstream hotspot name.

This change separates nearby screen hotspots. It does not change home-network credentials, Wi-Fi authentication, controller communication or pairing. Home-network connectivity must be diagnosed separately; this rename is not a claimed fix for an authentication failure.

K718 boot diagnostics subscribe before station startup so early event/reason codes are captured without logging credentials. The Brew rotary Profile shortcut from rc.14 remains present. Full build, software checks and any device observations must be tied to this candidate's exact identity. Normal display/controller/combined OTA remains disabled. See [Testing](TESTING.md).
