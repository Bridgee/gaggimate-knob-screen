"""Reviewed ESP-IDF 4.4 two-slot selectors; no device access.

Format/CRC: ESP-IDF bootloader_common_loader.c and app_update/otatool.py.
Only the K718 16 MB layout is supported. Unknown or unsettled states do not
silently become app0; explicit USB recovery is a separate operation.
"""
import binascii
import struct

ERASED = 0xffffffff
STATES = {0, 1, 2, 3, 4, ERASED}


def layout(parts):
    expected = {'nvs': (0x9000, 0x5000), 'otadata': (0xe000, 0x2000),
                'app0': (0x10000, 0x640000), 'app1': (0x650000, 0x640000)}
    for name, (offset, size) in expected.items():
        if (parts.get(name, {}).get('offset'), parts.get(name, {}).get('size')) != (offset, size):
            raise RuntimeError('Unsupported two-slot layout: ' + name)
    apps = [p for p in parts.values() if p.get('type') == 0]
    if len(apps) != 2 or parts['app0'].get('subtype') != 0x10 or parts['app1'].get('subtype') != 0x11:
        raise RuntimeError('Unsupported application partition types')


def record(sequence, state):
    if not 0 < sequence < ERASED or state not in STATES:
        raise RuntimeError('Unsupported OTA sequence/state')
    crc = binascii.crc32(struct.pack('<I', sequence), ERASED) & ERASED
    return struct.pack('<I20sII', sequence, b'\xff' * 20, state, crc)


def inspect(data):
    if len(data) != 8192:
        raise RuntimeError('Unsupported OTA metadata size')
    entries = []
    for copy in range(2):
        raw = data[copy * 4096:copy * 4096 + 32]
        seq, _, state, crc = struct.unpack('<I20sII', raw)
        erased = raw == b'\xff' * 32
        if not erased and (state not in STATES or (seq == 0 and raw != b'\x00' * 4 + b'\xff' * 28)):
            raise RuntimeError('Unknown OTA metadata; explicit review required')
        valid = seq != ERASED and crc == (binascii.crc32(raw[:4], ERASED) & ERASED)
        entries.append(dict(copy=copy, sequence=seq, state=state, crc_valid=valid,
                            bootable=valid and state not in (3, 4), erased=erased))
    return entries


def selected(data):
    entries = inspect(data)
    if any(e['crc_valid'] and e['state'] in (0, 1) for e in entries):
        raise RuntimeError('Unsettled OTA trial; use explicit recover, not update')
    active = [e for e in entries if e['bootable']]
    if not active:
        if data == b'\xff' * 8192:
            return dict(slot='app0', copy=None, sequence=0, state=ERASED)
        raise RuntimeError('No settled OTA selection; use explicit recover')
    chosen = max(active, key=lambda e: (e['sequence'], -e['copy']))
    if chosen['sequence'] == 0:
        raise RuntimeError('Only a vendor placeholder remains; use explicit recover')
    return dict(chosen, slot='app' + str((chosen['sequence'] - 1) % 2))


def trial(data):
    current = selected(data)
    if current['copy'] is None:
        raise RuntimeError('Initialize a settled app0 selection before a USB trial')
    seq = current['sequence'] + 1
    if seq >= ERASED:
        raise RuntimeError('OTA sequence exhausted; explicit recovery required')
    copy = 1 - current['copy']
    sector = record(seq, 0) + b'\xff' * (4096 - 32)
    result = bytearray(data)
    result[copy * 4096:(copy + 1) * 4096] = sector
    return 'app' + str((seq - 1) % 2), copy * 4096, sector, bytes(result)
