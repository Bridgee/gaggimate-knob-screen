"""Prepare an explicitly validation-only bundle; never an ordinary release/pair."""
import argparse,json,shutil
from pathlib import Path
from k718_flash import verified_release
from k718_repro import ARTIFACTS,digest
from k718 import ROOT,tool_env
from package_release import vendor_build_paths,artifact_issues
from check_public import content_issues

def package(source,output):
    source,manifest,_=verified_release(source,allow_test_image=True)
    if manifest.get('target')!='display-knob-validation' or not manifest.get('validation_only'):
        raise RuntimeError('Expected a validation-only package')
    if not ARTIFACTS.issubset(manifest['artifacts']):raise RuntimeError('Incomplete validation package')
    output=Path(output).resolve()
    if output.exists():raise RuntimeError('Choose a new output directory')
    keys=['schema','target','chip','upstream','port_version','release_channel','source_commit','source_tree','controller_protocol','application_offset','application_size','validation_only','screen_ota_standalone_qualification']
    metadata={key:manifest[key] for key in keys}
    metadata.update(hardware_verified=False,qualification='software-verified-validation-only',screen_ota_qualification=False,artifacts={n:manifest['artifacts'][n] for n in sorted(ARTIFACTS)})
    payload=(json.dumps(metadata,indent=2)+'\n').encode();issues=content_issues('release.json',payload)
    lock=json.loads((ROOT/'port/vendor-artifact-metadata.json').read_text())
    known=vendor_build_paths(Path(tool_env()['PLATFORMIO_CORE_DIR'])/'packages'/lock['package'],lock)
    for name in ARTIFACTS:issues.extend(artifact_issues(name,(source/name).read_bytes(),known))
    if issues:raise RuntimeError('\n'.join(issues))
    output.mkdir(parents=True)
    for name in ARTIFACTS:shutil.copyfile(source/name,output/name)
    (output/'release.json').write_bytes(payload)
    (output/'VALIDATION.md').write_text('# Validation-only firmware\n\nThis image has no physical controller connection. It uses local synthetic data, isolated settings/profiles and explicit screen-only OTA qualification. It cannot qualify BLE or brewing. Never publish it as a normal K718 release or matched pair.\n')
    (output/'SHA256SUMS').write_text(''.join(f'{digest(p)}  {p.name}\n' for p in sorted(output.iterdir()) if p.is_file()))
    return output
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('source');p.add_argument('--out',required=True);a=p.parse_args();print(package(a.source,a.out))
