#!/usr/bin/env python3
"""Generate/check feature illustrations from actual native LVGL renders."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SCENES = [
    ('menu-charging.png', 'doc-menu-charging.png', 'Dark', 'External power: lightning icon'),
    ('menu-battery.png', 'doc-menu-battery.png', 'Dark', 'Battery power: 60 percent fixture, fill-level icon'),
    ('knob-single.png', 'doc-knob-single.png', 'Light', 'Single temperature field selected automatically'),
    ('knob-multiple.png', 'doc-knob-multiple.png', 'Light', 'Temperature selected by touch among multiple fields'),
    ('standby-text.png', 'doc-standby-text.png', 'Dark', 'Custom standby text: MY COFFEE'),
    ('standby-clock.png', 'doc-standby-clock.png', 'Light', 'Centered clock without logo or custom text'),
]


def generate(check=False, rendered=False):
    if not rendered:
        subprocess.run([sys.executable, str(ROOT / 'test/knob_native/render_preview.py')], cwd=ROOT, check=True)
    build = Path(os.environ.get('K718_PREVIEW_DIR', ROOT / 'dist/preview'))
    output = ROOT / 'docs/images'
    manifest = {
        'kind': 'native-ui-render',
        'physical_capture': False,
        'simulated_values': True,
        'resolution': [360, 360],
        'generator': 'scripts/generate_doc_previews.py',
        'renderer': 'test/knob_native/render_preview.py',
        'images': {},
    }
    for name, frame, theme, purpose in SCENES:
        source = build / ('pages' if theme == 'Dark' else 'pages-light') / frame
        with Image.open(source) as image:
            if image.size != (360, 360) or image.mode != 'RGB':
                raise RuntimeError('Unexpected native frame format: ' + frame)
        data = source.read_bytes()
        manifest['images'][name] = {'scene': frame, 'theme': theme, 'purpose': purpose,
                                    'sha256': hashlib.sha256(data).hexdigest()}
        target = output / name
        if check:
            if not target.exists() or target.read_bytes() != data:
                raise RuntimeError('Stale documentation image: ' + name + '; run python scripts/generate_doc_previews.py')
        else:
            output.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    path = output / 'previews.json'
    data = (json.dumps(manifest, indent=2) + '\n').encode()
    if check:
        if not path.exists() or path.read_bytes() != data:
            raise RuntimeError('Stale documentation image manifest; regenerate previews')
    else:
        path.write_bytes(data)
    print(('PASS: verified' if check else 'Generated'), len(SCENES), 'purposeful native documentation images.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Verify checked-in images without changing them')
    parser.add_argument('--rendered', action='store_true', help='Use frames from the renderer already run in this build')
    args = parser.parse_args()
    generate(check=args.check, rendered=args.rendered)
