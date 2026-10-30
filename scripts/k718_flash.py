#!/usr/bin/env python3
"""Manifest-checked application-only updater. Defaults to an offline plan."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import site
import sys
from types import SimpleNamespace
from k718_lock import digest
from k718 import partition_entries, write_json, tool_env


def verified_release(folder):
    folder=Path(folder).resolve();manifest=json.loads((folder/'release.json').read_text())
    if manifest.get('schema')!=1 or manifest.get('target')!='display-knob' or manifest.get('chip')!='ESP32-S3':
        raise RuntimeError('Unsupported release target')
    for name,entry in manifest['artifacts'].items():
        path=(folder/name).resolve()
        if path.parent!=folder or not path.is_file() or path.stat().st_size!=entry['bytes'] or digest(path)!=entry['sha256']:
            raise RuntimeError('Artifact checksum/path mismatch: '+name)
    if manifest.get('audit_report_sha256') and digest(folder/'audit/report.json')!=manifest['audit_report_sha256']:
        raise RuntimeError('Build audit checksum mismatch')
    for name in ('firmware.bin','partitions.bin','boot_app0.bin'):
        if name not in manifest['artifacts']:raise RuntimeError('Missing artifact: '+name)
    parts=partition_entries((folder/'partitions.bin').read_bytes());app=parts.get('app0',{})
    if app.get('offset')!=manifest['application_offset'] or app.get('size')!=manifest['application_size']:
        raise RuntimeError('Manifest/partition mismatch')
    if manifest['artifacts']['firmware.bin']['bytes']>app['size']:raise RuntimeError('Application exceeds partition')
    return folder,manifest,parts


def qualification(folder, results, output):
    folder,manifest,_=verified_release(folder);results=json.loads(Path(results).read_text())
    checks=results.get('checks',{})
    required=manifest['required_hardware_checks']
    if any(checks.get(name) is not True for name in required):raise RuntimeError('Required hardware checks have not all passed')
    if results.get('firmware_sha256')!=manifest['artifacts']['firmware.bin']['sha256']:raise RuntimeError('Tested firmware SHA does not match this release')
    if results.get('controller_protocol')!=manifest['controller_protocol'] or not results.get('controller_identity') or not results.get('screen_mac'):
        raise RuntimeError('Record tested controller identity/protocol and screen MAC')
    output=Path(output)
    if output.exists():raise RuntimeError('Qualification output must be new')
    write_json(output,{'schema':1,'release_sha256':digest(folder/'release.json'),'firmware_sha256':manifest['artifacts']['firmware.bin']['sha256'],'source_tree':manifest['source_tree'],'qualified_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'results':results,'status':'hardware-verified'})
    return output


def flash(folder,port,expected_mac,backup,validation=False,receipt=None):
    folder,manifest,parts=verified_release(folder)
    if not validation:
        if not receipt:raise RuntimeError('Use a SHA-bound hardware qualification, or explicitly --validation for bench testing')
        evidence=json.loads(Path(receipt).read_text())
        if evidence.get('release_sha256')!=digest(folder/'release.json') or evidence.get('status')!='hardware-verified':
            raise RuntimeError('Qualification does not match release')
    # No controller auto-discovery; the operator must name the screen's port/MAC.
    if not port or not expected_mac or not backup:raise RuntimeError('Writing requires --port, --expected-mac and a new external --backup directory')
    backup=Path(backup).resolve()
    source=Path(__file__).resolve().parents[1]
    if backup==source or source in backup.parents or backup.exists():raise RuntimeError('Backup must be a new directory outside the source repository')
    core=Path(tool_env()['PLATFORMIO_CORE_DIR']);package=core/'packages/tool-esptoolpy'
    sys.path.insert(0,str(package));site.addsitedir(str(package/'_contrib'))
    from esptool.targets.esp32s3 import ESP32S3ROM
    from esptool.cmds import write_flash
    esp=ESP32S3ROM(port,115200);connected=False;written=False
    os.umask(0o077)
    try:
        esp.connect('usb_reset',attempts=3);connected=True
        mac=':'.join(f'{x:02x}' for x in esp.read_mac())
        if mac!=expected_mac.lower():raise RuntimeError('Wrong device MAC; no flash written')
        esp=esp.run_stub();esp.change_baud(921600);esp.flash_set_parameters(16*1024*1024)
        partition=bytes(esp.read_flash(0x8000,0x1000))
        if partition[:len((folder/'partitions.bin').read_bytes())]!=(folder/'partitions.bin').read_bytes():
            raise RuntimeError('Device partition table differs; explicit migration required')
        ota=parts.get('otadata')
        if not ota:raise RuntimeError('No OTA selection partition')
        current_ota=bytes(esp.read_flash(ota['offset'],ota['size']))
        # OTA selection/CRC semantics are intentionally not guessed. This updater
        # supports the known app0 baseline; other boot selections require review.
        if current_ota!=(folder/'boot_app0.bin').read_bytes():
            raise RuntimeError('OTA selection differs from known app0 baseline; no write performed')
        backup.mkdir(parents=True,mode=0o700)
        receipt_data={'screen_mac':mac,'port':port,'release_sha256':digest(folder/'release.json'),'firmware_sha256':manifest['artifacts']['firmware.bin']['sha256'],'validation_only':validation,'scope':'app0 only','backups':{},'written':False}
        snapshots={}
        for name,address,size in [('partition-table',0x8000,0x1000),('otadata',ota['offset'],ota['size']),('nvs',parts['nvs']['offset'],parts['nvs']['size']),('app0',manifest['application_offset'],manifest['application_size'])]:
            data=bytearray()
            for cursor in range(0,size,128*1024):
                count=min(128*1024,size-cursor);chunk=bytes(esp.read_flash(address+cursor,count))
                if len(chunk)!=count or esp.flash_md5sum(address+cursor,count)!=hashlib.md5(chunk).hexdigest():raise RuntimeError('Backup read verification failed: '+name)
                data.extend(chunk)
            snapshots[name]=bytes(data);path=backup/(name+'.bin');path.write_bytes(data)
            receipt_data['backups'][name]={'offset':address,'bytes':size,'sha256':digest(path)}
            write_json(backup/'flash.json',receipt_data)
        binary=(folder/'firmware.bin').read_bytes()
        receipt_data['write_attempted']=True
        write_json(backup/'flash.json',receipt_data)
        with (folder/'firmware.bin').open('rb') as stream:
            write_flash(esp,SimpleNamespace(addr_filename=[(manifest['application_offset'],stream)],compress=True,no_compress=False,no_stub=False,force=False,encrypt=False,encrypt_files=None,ignore_flash_encryption_efuse_setting=False,erase_all=False,flash_size='keep',flash_mode='keep',flash_freq='keep',chip='esp32s3'))
        written=True
        # esptool 4.9 retries mutate the caller's IS_STUB to False, then replace
        # only their local loader. The device still runs the stub. Rebind its
        # transport without resetting/running the app before preserved-region reads.
        if not esp.IS_STUB:
            esp=esp.STUB_CLASS(esp)
        if esp.flash_md5sum(manifest['application_offset'],len(binary))!=hashlib.md5(binary).hexdigest():raise RuntimeError('Independent application readback failed')
        for name in ('partition-table','otadata','nvs'):
            region=receipt_data['backups'][name]
            if bytes(esp.read_flash(region['offset'],region['bytes']))!=snapshots[name]:raise RuntimeError('Unexpected change outside app0: '+name)
        receipt_data.update(written=True,readback_verified=True,completed_at=datetime.datetime.now(datetime.timezone.utc).isoformat())
        write_json(backup/'flash.json',receipt_data)
        print('Application verified; settings, pairing, OTA selection and filesystem were not written.')
    finally:
        if connected:
            try:esp._port.setDTR(False);esp.hard_reset()
            finally:esp._port.close()
        if written:print('A write occurred; retain backup and flash receipt even if later verification failed.')


def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('plan');p.add_argument('release')
    p=sub.add_parser('qualify');p.add_argument('release');p.add_argument('--results',required=True);p.add_argument('--out',required=True)
    p=sub.add_parser('update');p.add_argument('release');p.add_argument('--write',action='store_true');p.add_argument('--port');p.add_argument('--expected-mac');p.add_argument('--backup');p.add_argument('--validation',action='store_true');p.add_argument('--qualification')
    args=parser.parse_args()
    try:
        if args.command=='qualify':print(qualification(args.release,args.results,args.out))
        elif args.command=='plan' or not args.write:
            folder,m,parts=verified_release(args.release)
            print(json.dumps({'release':str(folder),'firmware_sha256':m['artifacts']['firmware.bin']['sha256'],'upstream':m['upstream'],'controller_protocol':m['controller_protocol'],'write_region':parts['app0'],'hardware_qualified':False,'device_contacted':False},indent=2))
        else:flash(args.release,args.port,args.expected_mac,args.backup,args.validation,args.qualification)
    except (RuntimeError,KeyError,ValueError,OSError) as error:print('STOP:',error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':sys.exit(main())
