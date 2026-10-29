# Nanopb generator input

The PlatformIO Nanopb 0.4.92 package omits `generator/proto/nanopb_pb2.py`; its generator normally creates this file on first use.
This port pins the compatible generated descriptor used by the validated toolchain, so fresh installs and cached builds have the same dependency identity.

The descriptor is generated from Nanopb 0.4.92's `nanopb.proto` using protobuf 6.31.1 and is compatible with the locked runtime. Source, prepared-package and descriptor hashes are recorded in `port/dependencies.lock.json`.
Preparation accepts only the exact pristine package or exact prepared package; changes to any other dependency file are rejected. The original Nanopb license is retained here.
