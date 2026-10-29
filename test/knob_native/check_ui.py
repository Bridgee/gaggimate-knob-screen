"""Verify native resource integrity and preservation of upstream UI behavior."""
from pathlib import Path
import collections
import json
import re
import runpy
import struct

ROOT = Path(__file__).resolve().parents[2]
runpy.run_path(str(ROOT / 'scripts/generate_knob_ui.py'))['check']()
original = ROOT / 'src/display/ui/default/eez'
native = ROOT / 'src/display/ui/default/knob360'
a, b = (original / 'screens.c').read_text(), (native / 'screens.c').read_text()
handlers = r'static void (event_handler_\w+)\([^)]*\) \{.*?^\}'
original_handlers = {m[1]: m[0] for m in re.finditer(handlers, a, re.S | re.M)}
native_handlers = {m[1]: m[0] for m in re.finditer(handlers, b, re.S | re.M)}
assert len(original_handlers) == 64 and original_handlers == native_handlers
widgets = r'\b(lv_\w+_create)\('
assert collections.Counter(re.findall(widgets, a)) == collections.Counter(re.findall(widgets, b))

def assets(path):
    s = path.read_text()
    block = re.search(r'const uint8_t assets\[\d+\] = \{(.*?)\};', s, re.S)[1]
    return bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', block))

old, new = assets(original / 'ui.c'), assets(native / 'ui.c')
assert len(old) == len(new) == 21712
settings = 12 + struct.unpack_from('<i', new, 12)[0]
assert struct.unpack_from('<HH', new, settings) == (360, 360)
assert all(x == y or settings <= i < settings + 4 for i, (x, y) in enumerate(zip(old, new)))
assert set(re.findall(r'lv_img_set_zoom\(obj, (\d+)\)', b)) == {'192', '256'}
assert len(re.findall(r'lv_img_set_zoom\(obj, 192\)', b)) == 4  # Dynamic profile photos only.
fonts = set(map(int, re.findall(r'lv_obj_set_style_text_font\(obj, &lv_font_montserrat_(\d+)', b)))
assert fonts == {10, 12, 14, 16, 18, 26}
manifest = json.loads((native / 'manifest.json').read_text())
images = (native / 'images.c').read_text()
for symbol, info in manifest['resources'].items():
    array = re.search(r'static const uint8_t ' + symbol + r'_pixels\[\] = \{(.*?)\};', images, re.S)[1]
    count = len(re.findall(r'0x[0-9a-f]{2}', array))
    stride = 1 if info['format'] == 'LV_IMG_CF_ALPHA_8BIT' else 3
    assert count == info['bytes'] == info['width'] * info['height'] * stride
project = json.loads((ROOT / 'eez-ui/gaggimate-knob360.eez-project').read_text())
assert project['settings']['general']['displayWidth'] == project['settings']['general']['displayHeight'] == 360
assert len(project['userPages']) == 10
for page in project['userPages']:
    screen = page['components'][0]
    assert screen['width'] == screen['height'] == 360
driver = (ROOT / 'src/display/drivers/GuitionKnobDriver.cpp').read_text()
assert not any(s in driver for s in ('CanvasSize', 'scaleRegion', 'scaleStrip', 'canvasCoordinate'))
assert 'data->point.x = x;' in driver and 'data->point.y = y;' in driver
print('PASS: 64 handlers, all widget types, all flow bytecode retained; native fonts/assets/metadata; direct pixels and touch.')
