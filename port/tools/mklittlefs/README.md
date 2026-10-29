# mklittlefs-k718-repro1

Pinned build dependency for deterministic LittleFS images, not device firmware.

- Upstream mklittlefs: `943d2f76736eef6630edd4933e42c3016194770b`.
- LittleFS: `abd90cb84c818a663b584575b019258d01d0065e` (v2.0.5).
- The source archive includes the reproducibility patch, TCLAP, LittleFS and licenses.
- The Darwin arm64 binary requires macOS 26 or later and was built with Apple clang 21.0.0.

`port/reproducibility.json` verifies the source, patch, archive and executable hashes. The build prepares a private cache without changing PlatformIO's installed tool.
Corrupt inputs and unsupported platforms are rejected.

To rebuild for review, extract `source.tar.gz` into a new directory and run:

```sh
make VERSION=0.2.3-k718-repro1 LITTLEFS_VERSION=v2.0.5
```

Review compiler identity and image consistency before adding a new platform package to the lock. Do not replace a verified package silently.
