"""Headless visual test using actual LVGL8/EEZ native360 pages, no hardware."""
from pathlib import Path
import concurrent.futures
import hashlib
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BUILD = Path(os.environ.get('K718_PREVIEW_DIR', ROOT / 'dist/preview'))
INC = BUILD / 'include'
PAGES = BUILD / 'pages'
LVGL = ROOT / '.pio/libdeps/display-knob/lvgl'
EEZ = ROOT / 'src/display/ui/default/eez'
NATIVE = ROOT / 'src/display/ui/default/knob360'
for p in (BUILD, INC, PAGES, BUILD / 'obj'): p.mkdir(parents=True, exist_ok=True)
config = (ROOT / 'src/display/lv_conf.h').read_text()
config = config.replace('#define LV_MEM_CUSTOM 1', '#define LV_MEM_CUSTOM 0')
config = re.sub(r'#define LV_MEM_SIZE\s+.*', '#define LV_MEM_SIZE (8 * 1024 * 1024)', config)
(INC / 'lv_conf.h').write_text(config)
(INC / 'Arduino.h').write_text('#pragma once\n#include <stdint.h>\n#ifdef __cplusplus\nextern "C" {\n#endif\nuint32_t millis(void);\n#ifdef __cplusplus\n}\n#endif\n')

actions = (EEZ / 'actions.cpp').read_text()
names = re.findall(r'extern void (action_\w+)\(', (EEZ / 'actions.h').read_text())

def function(name):
    match = re.search(r'(?:static )?void ' + name + r'\([^)]*\)\s*\{', actions)
    start = match.start(); end = match.end(); depth = 1
    while depth:
        if actions[end] == '{': depth += 1
        elif actions[end] == '}': depth -= 1
        end += 1
    return actions[start:end] + '\n'

stubs = '#include "actions.h"\n#include "screens.h"\n#include "NativeResolution.h"\n#include <cmath>\n'
stubs += function('applyClickArea') + function('suppressMeterTicks')
for name in names:
    stubs += function(name) if name in ('action_on_meter_draw', 'action_on_screen_load') else 'void ' + name + '(lv_event_t *) {}\n'
(BUILD / 'actions.cpp').write_text(stubs)
flags = ['-O1', '-g', '-DGAGGIMATE_KNOB', '-DLV_CONF_INCLUDE_SIMPLE', '-DLV_LVGL_H_INCLUDE_SIMPLE',
         '-I' + str(INC), '-I' + str(LVGL), '-I' + str(EEZ), '-I' + str(EEZ.parent), '-I' + str(ROOT / 'src'), '-Wno-unknown-warning-option']
sources = sorted((LVGL / 'src').rglob('*.c')) + [NATIVE / x for x in ('screens.c', 'ui.c', 'images.c')]
sources += [EEZ / 'styles.c', EEZ / 'images.c', EEZ / 'eez-flow.cpp', BUILD / 'actions.cpp', ROOT / 'test/knob_native/preview.cpp']
sources += [EEZ.parent / 'KnobMenuBattery.cpp']
sources += [EEZ.parent / 'KnobBatteryImages.c', EEZ.parent / 'KnobAdjustment.cpp', EEZ.parent / 'StandbyDisplay.cpp', ROOT / 'test/knob_native/check_ui_extensions.cpp']
config_key = hashlib.sha256(config.encode()).hexdigest()

def compile_source(source):
    key = hashlib.sha256((str(source) + config_key).encode()).hexdigest()[:24]
    obj = BUILD / 'obj' / (key + '.o')
    # Recompile: header/theme changes must not reuse stale host objects.
    cpp = source.suffix == '.cpp'
    command = ['clang++' if cpp else 'clang', '-std=c++17' if cpp else '-std=c99', *flags, '-c', str(source), '-o', str(obj)]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode: raise RuntimeError(str(source) + '\n' + result.stderr[-6000:])
    return str(obj)

with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool: objects = list(pool.map(compile_source, sources))
subprocess.run(['clang++', *objects, '-o', str(BUILD / 'preview')], check=True)
subprocess.run([str(BUILD / 'preview'), str(PAGES), '0'], check=True)
LIGHT = BUILD / 'pages-light'
LIGHT.mkdir(exist_ok=True)
subprocess.run([str(BUILD / 'preview'), str(LIGHT), '1'], check=True)
from PIL import Image, ImageDraw
names = ['Standby', 'Brew', 'Status', 'Menu', 'Steam', 'Water', 'Profiles', 'Grind', 'Info', 'New profile']
sheet = Image.new('RGB', (5 * 380, 2 * 400), '#232323'); draw = ImageDraw.Draw(sheet)
for i, name in enumerate(names):
    img = Image.open(PAGES / ('page-%02d.ppm' % (i + 1)))
    img.save(PAGES / ('page-%02d.png' % (i + 1)))
    x, y = (i % 5) * 380 + 10, (i // 5) * 400 + 10
    sheet.paste(img, (x, y)); draw.text((x + 5, y + 367), name, fill='white')
sheet.save(BUILD / 'native360-pages.png')
print('Preview:', BUILD / 'native360-pages.png')

for directory in (PAGES, LIGHT):
    for path in directory.glob("*.ppm"):
        Image.open(path).save(path.with_suffix(".png"))
