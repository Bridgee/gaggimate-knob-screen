"""Exercise the actual firmware-generated nanopb schema and queue on the host.

Run after the firmware step: a cold build must generate its own schema first.
No radio, RTOS, controller endpoint or physical outputs are simulated here.
"""
from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'dist/protocol-check'
GENERATED = ROOT / '.pio/build/display-knob/nanopb/generated-src'
NANOPB = ROOT / '.pio/libdeps/display-knob/Nanopb'
BUILD.mkdir(parents=True, exist_ok=True)
protocol = json.loads((ROOT / 'port/config.json').read_text())['controller_protocol']
flags = ['-g', '-O1', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
         '-I' + str(GENERATED), '-I' + str(NANOPB),
         '-I' + str(ROOT / 'lib/NanoPbComm/src')]
objects = []
for source in [GENERATED / 'gaggimate.pb.c', *[NANOPB / name for name in ('pb_common.c', 'pb_encode.c', 'pb_decode.c')]]:
    if not source.is_file():
        raise RuntimeError('Build firmware before checking its generated protocol: ' + str(source))
    obj = BUILD / (source.stem + '.o')
    subprocess.run(['clang', '-std=c99', *flags, '-c', str(source), '-o', str(obj)], check=True)
    objects.append(str(obj))
binary = BUILD / 'check-protocol'
subprocess.run(['clang++', '-std=c++17', *flags, '-DREVIEWED_PROTOCOL=' + str(protocol),
                str(ROOT / 'test/knob_native/check_protocol.cpp'), *objects, '-o', str(binary)], check=True)
subprocess.run([str(binary)], check=True)
