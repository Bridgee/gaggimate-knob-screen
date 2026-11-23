"""Keep controller, protocol, PID and process execution at the reviewed upstream."""
import json
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from k718_validation_source import production_source

ROOT = Path(__file__).resolve().parents[2]
base = json.loads((ROOT / 'port/config.json').read_text())['upstream']['commit']
domains = ['src/controller', 'lib/NanoPbComm', 'lib/GaggiMateController/src',
           'lib/NayrodPID/src', 'src/display/core/process']
names = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', base, '--', *domains], cwd=ROOT).decode().splitlines()
assert names
for name in names:
    expected = subprocess.check_output(['git', 'show', base + ':' + name], cwd=ROOT)
    actual = (ROOT / name).read_bytes()
    if name == 'lib/GaggiMateController/src/peripherals/HardwareScale.cpp':
        # Drop only the original author's machine-local filepath comment.
        assert expected.startswith(b'// filepath: ')
        expected = expected.split(b'\n', 1)[1]
    if name in {'lib/NanoPbComm/src/GaggiMateClient.h', 'lib/NanoPbComm/src/GaggiMateClient.cpp'}:
        actual, expected = production_source(actual), production_source(expected)
    assert actual == expected, 'Upstream production control source changed: ' + name
print(f'PASS: {len(names)} upstream controller/protocol/PID/process files retained; one non-code filepath comment removed; two display facade files compared with validation-only guards disabled.')
