"""Normalize compiler metadata and include the epoch in library cache signatures."""
import os
from pathlib import Path
import sys
Import('env')
root = Path(env.subst('$PROJECT_DIR')).resolve()
sys.path.insert(0, str(root / 'scripts'))
from k718_repro import compiler_settings, prepare_tool

tools = Path(os.environ.get('K718_TOOLS', root.parent / '.tooling')).resolve()
core = Path(os.environ.get('PLATFORMIO_CORE_DIR', tools / 'pio-core')).resolve()
settings = compiler_settings(root, tools, core, os.environ.get('SOURCE_DATE_EPOCH', ''))
env.Append(CPPDEFINES=[settings['define']])
env.Append(CCFLAGS=settings['flags'], ASPPFLAGS=settings['flags'])
identity = prepare_tool(root, tools)
env['K718_REPRO_FS_TOOL'] = identity['binary']
print('Reproducible build: epoch in cache signature; normalized paths; verified managed filesystem tool')
