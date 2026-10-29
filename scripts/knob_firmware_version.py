"""Identify port builds by official base, port version and committed source tree."""
import datetime
import json
import os
from pathlib import Path
import subprocess

Import('env')
root = Path(env.subst('$PROJECT_DIR'))
def git(*args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()
config = json.loads((root / 'port/config.json').read_text())
tree = git('rev-parse', 'HEAD^{tree}')[:8]
dirty = bool(git('status', '--porcelain'))
version = f"{config['upstream']['version']}-k718.{config['port_version']}+{tree}"
if dirty: version += '.dirty'
epoch = int(os.environ.get('SOURCE_DATE_EPOCH', git('show', '-s', '--format=%at', 'HEAD')))
timestamp = datetime.datetime.fromtimestamp(epoch, datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
(root / 'src/version.h').write_text(
    f'#pragma once\n#define BUILD_GIT_VERSION "{version}"\n'
    f'#define BUILD_UPSTREAM_VERSION "{config["upstream"]["version"]}"\n'
    f'#define BUILD_TIMESTAMP "{timestamp}"\n'
)
print('K718 build version:', version)
