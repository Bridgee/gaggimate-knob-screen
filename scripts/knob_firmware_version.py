"""Identify port builds by official base, port version and committed source tree."""
import datetime
import json
import os
import sys
from pathlib import Path
import subprocess

Import('env')
root = Path(env.subst('$PROJECT_DIR'))
sys.path.insert(0, str(root / 'scripts'))
from k718_ota_config import qualification_enabled
def git(*args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()
config = json.loads((root / 'port/config.json').read_text())
bench_ota = qualification_enabled(config)
validation = env.subst('$PIOENV') == 'display-knob-validation'
if validation and (config.get('release_channel') != 'candidate' or '-' not in config['port_version']):
    raise RuntimeError('Validation firmware requires a candidate prerelease')
tree = git('rev-parse', 'HEAD^{tree}')[:8]
dirty = bool(git('status', '--porcelain'))
version = f"{config['upstream']['version']}-k718.{config['port_version']}+{tree}"
if validation: version += '.validation'
if dirty: version += '.dirty'
epoch = int(os.environ.get('SOURCE_DATE_EPOCH', git('show', '-s', '--format=%at', 'HEAD')))
timestamp = datetime.datetime.fromtimestamp(epoch, datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
(root / 'src/version.h').write_text(
    f'#pragma once\n#define BUILD_GIT_VERSION "{version}"\n'
    f'#define BUILD_UPSTREAM_VERSION "{config["upstream"]["version"]}"\n'
    f'#define BUILD_PORT_VERSION "{config["port_version"]}"\n'
    f'#define BUILD_RELEASE_CHANNEL "{config.get("release_channel", "development")}"\n'
    f'#define BUILD_SCREEN_OTA_QUALIFICATION {int(bench_ota and not validation)}\n'
    f'#define BUILD_SCREEN_OTA_STANDALONE_QUALIFICATION {int(validation)}\n'
    f'#define BUILD_FIRMWARE_TARGET {chr(34)}{"display-knob-validation" if validation else "display-knob"}{chr(34)}\n'
    f'#define BUILD_TIMESTAMP "{timestamp}"\n'
)
print('K718 build version:', version)
