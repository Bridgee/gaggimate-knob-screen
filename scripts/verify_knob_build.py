"""K718 must never silently build the upstream empty-web placeholder."""
from pathlib import Path
import re
Import('env')
root = Path(env.subst('$PROJECT_DIR'))
header = root / 'src/display/webassets/web_ui_manifest.h'
blob = root / 'src/display/webassets/web_ui.bin'
match = re.search(r'WEB_ASSETS_COUNT\s*=\s*(\d+)', header.read_text()) if header.exists() else None
if not match or int(match[1]) < 1 or not blob.exists() or blob.stat().st_size < 1024:
    raise RuntimeError('K718 requires real embedded web assets. Run scripts/k718.py build or build_webui.sh first.')
