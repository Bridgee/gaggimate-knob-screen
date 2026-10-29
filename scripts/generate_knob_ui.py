"""Generate native K718 geometry/resources at build time, never in the LCD flush.

Run with Python + Pillow. Original EEZ generated sources remain upstream-owned;
the K718 build compiles this deterministic derivative instead. --check needs
only the standard library and rejects stale output before compiling firmware.
"""
import argparse
import base64
import copy
import hashlib
import io
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
EEZ = ROOT / 'src/display/ui/default/eez'
OUT = ROOT / 'src/display/ui/default/knob360'
PROJECT = ROOT / 'eez-ui/gaggimate.eez-project'
NATIVE_PROJECT = ROOT / 'eez-ui/gaggimate-knob360.eez-project'
FONT = {14: 10, 16: 12, 18: 14, 20: 16, 24: 18, 34: 26}
LINE_HEIGHT = {10: 11, 12: 15, 14: 16, 16: 18, 18: 21, 26: 29}  # LVGL8.4 built-in fonts


def px(value):
    n = int(value)
    return (1 if n >= 0 else -1) * ((abs(n) * 3 + 2) // 4)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inputs():
    return [Path(__file__), PROJECT, EEZ / 'screens.c', EEZ / 'ui.c', EEZ / 'images.c', *sorted((EEZ / 'images').glob('*.c'))]


def check():
    m = json.loads((OUT / 'manifest.json').read_text())
    for group in ('inputs', 'outputs'):
        for rel, expected in m[group].items():
            if digest(ROOT / rel) != expected:
                raise SystemExit('Stale native UI: ' + rel + '; run scripts/generate_knob_ui.py')
    print('Native360 UI verified: 10 pages, native fonts/icons, flow logic retained.')


def rewrite_calls(text, rules):
    counts = {}
    pattern = re.compile(r'\b(' + '|'.join(rules) + r')\s*\(')
    result = []; cursor = 0
    for m in list(pattern.finditer(text)):
        name = m[1]; start = m.end(); depth = 1; quote = None; escaped = False; args = []; part = start; pos = start
        while depth:
            c = text[pos]
            if quote:
                if escaped: escaped = False
                elif c == '\\': escaped = True
                elif c == quote: quote = None
            elif c in ('"', "'"): quote = c
            elif c == '(': depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0: args.append(text[part:pos].strip())
            elif c == ',' and depth == 1:
                args.append(text[part:pos].strip()); part = pos + 1
            pos += 1
        for index in rules[name]:
            if re.fullmatch(r'-?\d+', args[index]): args[index] = str(px(args[index]))
            elif args[index] not in ('LV_SIZE_CONTENT', 'LV_RADIUS_CIRCLE'):
                raise ValueError('Unhandled spatial expression: ' + name + ' ' + args[index])
        result.extend((text[cursor:m.start()], name + '(' + ', '.join(args) + ')'))
        cursor = pos; counts[name] = counts.get(name, 0) + 1
    result.append(text[cursor:])
    return ''.join(result), counts


def generate():
    from PIL import Image
    OUT.mkdir(exist_ok=True)
    project = json.loads(PROJECT.read_text())
    symbols = dict(re.findall(r'\{"([^"]+)", &(img_\w+)\}', (EEZ / 'images.c').read_text()))
    bitmaps = {x['name']: x for x in project['bitmaps']}
    originals = {name: Image.open(io.BytesIO(base64.b64decode(item['image'].split(',', 1)[1]))).convert('RGBA')
                 for name, item in bitmaps.items()}
    formats = {}
    for path in (EEZ / 'images').glob('*.c'):
        source = path.read_text()
        symbol = re.search(r'const lv_img_dsc_t (img_\w+)', source)[1]
        formats[symbol] = re.search(r'\.header.cf\s*=\s*(\w+)', source)[1]
    variants = {}; images = {}; names_by_symbol = {v: k for k, v in symbols.items()}

    def bake(name, zoom=256):
        source_symbol = symbols[name]
        symbol = source_symbol if zoom in (255, 256) else source_symbol + '_z' + str(zoom)
        if symbol not in images:
            original = originals[name]
            size = tuple(max(1, (v * 3 * zoom + 512) // 1024) for v in original.size)
            image = original.resize(size, Image.Resampling.LANCZOS)
            images[symbol] = (image, formats[source_symbol])
            if symbol != source_symbol: variants[(name, zoom)] = symbol
        return symbol

    for name in bitmaps: bake(name)
    source = (EEZ / 'screens.c').read_text()
    zoom_pattern = re.compile(r'lv_img_set_src\(obj, &(img_\w+)\);((?:(?!lv_img_set_src|lv_img_create).)*?)lv_img_set_zoom\(obj,\s*(\d+)\);', re.S)

    def bake_zoom(m):
        symbol = bake(names_by_symbol[m[1]], int(m[3]))
        return 'lv_img_set_src(obj, &' + symbol + ');' + m[2] + 'lv_img_set_zoom(obj, 256);'

    source, zoom_count = zoom_pattern.subn(bake_zoom, source)
    if zoom_count != 20: raise ValueError('Review changed upstream image zoom sites')
    # Four image placeholders receive user/profile pictures dynamically, rather
    # than a compiled icon. Retain individual image zoom for those only.
    source, dynamic_zoom_count = re.subn(r'lv_img_set_zoom\(obj, 255\);', 'lv_img_set_zoom(obj, 192);', source)
    if dynamic_zoom_count != 4: raise ValueError('Review dynamic profile image sites')
    rules = {'lv_obj_set_pos': [1, 2], 'lv_obj_set_size': [1, 2], 'lv_qrcode_create': [1],
             'lv_meter_set_scale_ticks': [3, 4], 'lv_meter_set_scale_major_ticks': [3, 4, 6],
             'lv_meter_add_needle_img': [3, 4], 'lv_meter_add_scale_lines': [5]}
    for style in ('pad_left', 'pad_top', 'pad_right', 'pad_bottom', 'pad_row', 'pad_column',
                  'border_width', 'radius', 'shadow_width', 'text_letter_space'):
        rules['lv_obj_set_style_' + style] = [1]
    source, counts = rewrite_calls(source, rules)
    if counts['lv_obj_set_size'] != 239 or counts['lv_obj_set_pos'] != 239:
        raise ValueError('Review changed upstream widget geometry')
    source = re.sub(r'(lv_obj_set_style_text_font\(obj, &lv_font_montserrat_)(\d+)',
                    lambda m: m[1] + str(FONT[int(m[2])]), source)
    # A one-pixel font/box rounding difference can make SCROLL_CIRCULAR
    # animate vertically. Reserve the actual native font line height.
    label_height_fixes = 0
    label_blocks = re.compile(r'lv_obj_t \*obj = lv_label_create\(parent_obj\);(?:(?!lv_obj_t \*obj =).)*', re.S)

    def fit_label(m):
        nonlocal label_height_fixes
        block = m[0]
        size = re.search(r'lv_obj_set_size\(obj, ([^,]+), (\d+)\);', block)
        font = re.search(r'lv_obj_set_style_text_font\(obj, &lv_font_montserrat_(\d+)', block)
        if size and font and int(size[2]) < LINE_HEIGHT[int(font[1])]:
            block = block[:size.start(2)] + str(LINE_HEIGHT[int(font[1])]) + block[size.end(2):]
            label_height_fixes += 1
        return block

    source = label_blocks.sub(fit_label, source)

    def local_headers(text):
        return re.sub(r'#include "(\w+\.h)"', lambda m: '#include "' +
                      (m[1] if m[1] == 'images.h' else '../eez/' + m[1]) + '"', text)

    (OUT / 'screens.c').write_text('/* Generated native360 geometry; see scripts/generate_knob_ui.py. */\n' + local_headers(source))
    # Asset settings are relative-pointer data. Update only its display size;
    # all action/variable indices, expressions and flow bytecode stay identical.
    ui = (EEZ / 'ui.c').read_text()
    match = re.search(r'(const uint8_t assets\[\d+\] = \{)(.*?)(\};)', ui, re.S)
    binary = bytearray(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', match[2]))
    settings = 12 + struct.unpack_from('<i', binary, 12)[0]
    if struct.unpack_from('<HH', binary, settings) != (480, 480): raise ValueError('Unexpected EEZ assets settings')
    struct.pack_into('<HH', binary, settings, 360, 360)
    formatted = '\n' + '\n'.join('    ' + ', '.join('0x%02x' % n for n in binary[i:i + 24]) + ','
                                   for i in range(0, len(binary), 24)) + '\n'
    ui = ui[:match.start()] + match[1] + formatted + match[3] + ui[match.end():]
    (OUT / 'ui.c').write_text(local_headers(ui))
    header = '#pragma once\n#include "../eez/images.h"\n#ifdef __cplusplus\nextern "C" {\n#endif\n'
    for symbol in sorted(images):
        if symbol not in names_by_symbol: header += 'extern const lv_img_dsc_t ' + symbol + ';\n'
    header += '#ifdef __cplusplus\n}\n#endif\n'
    (OUT / 'images.h').write_text(header)
    code = '#include "images.h"\n'
    resource_info = {}
    for symbol, (image, fmt) in sorted(images.items()):
        if fmt == 'LV_IMG_CF_ALPHA_8BIT': data = image.getchannel('A').tobytes()
        elif fmt == 'LV_IMG_CF_TRUE_COLOR_ALPHA':
            data = bytearray()
            for r, g, b, a in image.getdata():
                color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                data.extend((color & 255, color >> 8, a))
        else: raise ValueError(fmt)
        code += '\nstatic const uint8_t ' + symbol + '_pixels[] = {\n'
        code += '\n'.join('    ' + ', '.join('0x%02x' % n for n in data[i:i + 24]) + ',' for i in range(0, len(data), 24))
        code += '\n};\nconst lv_img_dsc_t ' + symbol + ' = {\n'
        code += '    .header = {.cf = %s, .always_zero = 0, .reserved = 0, .w = %d, .h = %d},\n' % (fmt, *image.size)
        code += '    .data_size = %d, .data = %s_pixels\n};\n' % (len(data), symbol)
        resource_info[symbol] = {'width': image.width, 'height': image.height, 'format': fmt, 'bytes': len(data)}
    (OUT / 'images.c').write_text(code)

    native = copy.deepcopy(project)
    native['settings']['general'].update(displayWidth=360, displayHeight=360)
    # Opening this review project in EEZ must never overwrite upstream files.
    native['settings']['build']['destinationFolder'] = 'native360-editor-export'
    spatial = {'left', 'top', 'width', 'height', 'pivotX', 'pivotY'}
    style_spatial = {'pad_left', 'pad_top', 'pad_right', 'pad_bottom', 'pad_row', 'pad_column', 'border_width',
                     'radius', 'shadow_width', 'text_letter_space', 'minorTickLineWidth', 'minorTickLength',
                     'majorTickWidth', 'majorTickLength', 'widthModifier', 'labelGap'}

    def adapt(item):
        if isinstance(item, dict):
            if item.get('type') == 'LVGLImageWidget' and item.get('image') in symbols:
                name, zoom = item['image'], int(item.get('zoom', 256))
                if (name, zoom) in variants: item['image'] = name + '-z' + str(zoom)
                item['zoom'] = 256
            elif item.get('type') == 'LVGLImageWidget' and 'zoom' in item:
                item['zoom'] = px(item['zoom'])
            for key, value in list(item.items()):
                if key in spatial | style_spatial and isinstance(value, (int, float)) and not isinstance(value, bool): item[key] = px(value)
                elif key == 'text_font' and isinstance(value, str):
                    item[key] = re.sub(r'MONTSERRAT_(\d+)', lambda m: 'MONTSERRAT_' + str(FONT.get(int(m[1]), int(m[1]))), value)
                else: adapt(value)
            if item.get('type') == 'LVGLLabelWidget' and item.get('heightUnit') != 'content':
                font = item.get('localStyles', {}).get('definition', {}).get('MAIN', {}).get('DEFAULT', {}).get('text_font', '')
                m = re.fullmatch(r'MONTSERRAT_(\d+)', font)
                if m and isinstance(item.get('height'), int): item['height'] = max(item['height'], LINE_HEIGHT[int(m[1])])
        elif isinstance(item, list):
            for value in item: adapt(value)

    for key in ('userPages', 'userWidgets', 'lvglStyles'): adapt(native[key])
    for entry in native['bitmaps']:
        buffer = io.BytesIO(); images[symbols[entry['name']]][0].save(buffer, format='PNG')
        entry['image'] = 'data:image/png;base64,' + base64.b64encode(buffer.getvalue()).decode()
    for (name, zoom), symbol in variants.items():
        entry = copy.deepcopy(bitmaps[name]); entry['name'] = name + '-z' + str(zoom)
        # Derivative resources get deterministic, distinct IDs.
        import uuid
        entry['objID'] = str(uuid.uuid5(uuid.NAMESPACE_URL, 'gaggimate-knob360/' + entry['name']))
        buffer = io.BytesIO(); images[symbol][0].save(buffer, format='PNG')
        entry['image'] = 'data:image/png;base64,' + base64.b64encode(buffer.getvalue()).decode()
        native['bitmaps'].append(entry)
    NATIVE_PROJECT.write_text(json.dumps(native, ensure_ascii=False, indent=2) + '\n')
    outputs = [OUT / x for x in ('screens.c', 'ui.c', 'images.c', 'images.h')] + [NATIVE_PROJECT]
    manifest = {'resolution': [360, 360], 'pages': 10, 'widgets': 2, 'font_map': FONT, 'geometry_calls': counts,
                'baked_zoom_sites': zoom_count, 'dynamic_profile_image_sites': dynamic_zoom_count,
                'label_height_fixes': label_height_fixes,
                'resources': resource_info, 'flow_metadata_patch': [settings, settings + 4],
                'inputs': {str(p.relative_to(ROOT)): digest(p) for p in inputs()},
                'outputs': {str(p.relative_to(ROOT)): digest(p) for p in outputs}}
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    check()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('--check', action='store_true')
    if parser.parse_args().check: check()
    else: generate()
