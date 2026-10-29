"""Prepare a verified screen/controller pair. Never flashes or enables OTA."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile
import urllib.request

from check_public import content_issues
from k718_flash import verified_release
from k718_lock import digest

ROOT = Path(__file__).resolve().parents[1]


class HttpsRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if not newurl.startswith('https://'):
            raise RuntimeError('Refusing non-HTTPS controller download redirect')
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def checked_policy(policy):
    asset = policy['asset']
    if (policy.get('schema') != 1 or policy.get('repository') != 'jniebuhr/gaggimate'
            or not re.fullmatch(r'v\d+\.\d+\.\d+', policy['tag'])
            or not re.fullmatch(r'[0-9a-f]{40}', policy['commit'])
            or asset['name'] != 'board-firmware.bin'
            or asset['url'] != f"https://github.com/{policy['repository']}/releases/download/{policy['tag']}/{asset['name']}"
            or not re.fullmatch(r'[0-9a-f]{64}', asset['sha256'])
            or not 288 < asset['bytes'] <= 8 * 1024 * 1024
            or policy.get('installation_allowed') is not False):
        raise RuntimeError('Invalid pinned official controller policy')
    return asset


def check_image(data):
    # ESP32-S3 app header, app descriptor and appended image digest. The
    # Arduino vendor descriptor is generic; release identity comes from SHA-256.
    if (len(data) < 288 or data[0] != 0xe9 or int.from_bytes(data[12:14], 'little') != 9
            or data[23] != 1 or data[32:36] != bytes.fromhex('3254cdab')
            or hashlib.sha256(data[:-32]).digest() != data[-32:]):
        raise RuntimeError('Invalid ESP32-S3 application image/digest')


def official_controller(policy, local=None):
    asset = checked_policy(policy)
    if local is None:
        opener = urllib.request.build_opener(HttpsRedirect())
        with opener.open(asset['url'], timeout=60) as response:
            if response.status != 200 or not response.url.startswith('https://'):
                raise RuntimeError('Invalid controller download response')
            data = response.read(asset['bytes'] + 1)
    else:
        path = Path(local)
        if not path.is_file() or path.is_symlink() or path.stat().st_size != asset['bytes']:
            raise RuntimeError('Controller checksum/size mismatch')
        data = path.read_bytes()
    if len(data) != asset['bytes'] or hashlib.sha256(data).hexdigest() != asset['sha256']:
        raise RuntimeError('Controller checksum/size mismatch')
    check_image(data)
    return data


def package(source, output, controller=None, policy=None):
    source, screen, _ = verified_release(source)
    policy = policy if policy is not None else json.loads((ROOT / 'port/controller-release.json').read_text())
    checked_policy(policy)
    if (screen['upstream']['commit'] != policy['commit']
            or screen['upstream']['version'] != policy['tag']
            or screen['controller_protocol'] != policy['controller_protocol']):
        raise RuntimeError('Screen/controller compatibility mismatch; review the pinned pair')
    output = Path(output).resolve()
    if output.exists():
        raise RuntimeError('Choose a new pair output directory')
    data = official_controller(policy, controller)
    screen_data = (source / 'firmware.bin').read_bytes()
    check_image(screen_data)
    controller_name, screen_name = 'board-firmware.bin', 'display-knob-firmware.bin'
    pair = {
        'schema': 1, 'kind': 'gaggimate-knob-screen-firmware-pair',
        'release_repository': 'Bridgee/gaggimate-knob-screen',
        'screen': {
            'version': screen['port_version'], 'upstream_version': screen['upstream']['version'],
            'target': screen['target'], 'board': 'Guition-K718', 'chip': screen['chip'],
            'source_commit': screen['source_commit'], 'source_tree': screen['source_tree'],
            'application_offset': screen['application_offset'], 'application_size': screen['application_size'],
            'asset': dict(screen['artifacts']['firmware.bin'], name=screen_name),
        },
        'controller': {
            'version': policy['tag'], 'board': policy['board'], 'chip': 'ESP32-S3',
            'origin': 'official-release-unmodified', 'upstream_repository': policy['repository'],
            'upstream_commit': policy['commit'], 'upstream_release_id': policy['release_id'],
            'upstream_asset_id': policy['asset']['id'], 'upstream_asset_url': policy['asset']['url'],
            'asset': {key: policy['asset'][key] for key in ('name', 'bytes', 'sha256')},
        },
        'compatibility': {
            'controller_protocol': policy['controller_protocol'],
            'upstream_commit': policy['commit'], 'status': 'package-verified',
            'controller_ota_verified': False, 'screen_ota_verified': False,
        },
        'installation_allowed': False,
    }
    payload = (json.dumps(pair, indent=2) + '\n').encode()
    # Inspect without rewriting either image. TLS parser delimiters, if present,
    # are constants rather than private keys; all other publication rules apply.
    controller_view = re.sub(rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----\x00',
                             b'<PEM-parser-delimiter>\x00', data)
    issues = content_issues('pair.json', payload) + content_issues(controller_name, controller_view)
    if issues:
        raise RuntimeError('\n'.join(issues))
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.pair-', dir=output.parent) as temporary:
        staging = Path(temporary) / 'package'
        staging.mkdir()
        (staging / screen_name).write_bytes(screen_data)
        (staging / controller_name).write_bytes(data)
        (staging / 'pair.json').write_bytes(payload)
        (staging / 'NOTICE.md').write_text(
            '# Firmware pair\n\n'
            'The controller image is the unmodified official GaggiMate release. '
            'The screen image is the independent K718 adaptation maintained by Bridgee.\n\n'
            'Upstream source: https://github.com/jniebuhr/gaggimate\n'
            'Port source: https://github.com/Bridgee/gaggimate-knob-screen\n\n'
            'GaggiMate is distributed under CC BY-NC-SA 4.0; third-party notices remain applicable. '
            'See LICENSE.md and the source repositories.\n\n'
            'This package prepares distribution inputs. OTA installation remains disabled. '
            'Software package checks do not qualify an on-device OTA transfer. '
            'Use the separate display USB bundle for a first installation or checked application update.\n')
        shutil.copyfile(ROOT / 'LICENSE.md', staging / 'LICENSE.md')
        (staging / 'SHA256SUMS').write_text(''.join(
            f'{digest(path)}  {path.name}\n' for path in sorted(staging.iterdir()) if path.is_file()))
        staging.rename(output)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('screen_package', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--controller', type=Path, help='Offline original asset; the pinned hash is still required')
    args = parser.parse_args()
    try:
        print(package(args.screen_package, args.out, args.controller))
    except (RuntimeError, OSError, KeyError, ValueError) as error:
        parser.exit(1, 'STOP: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
