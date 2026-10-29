#!/usr/bin/env python3
"""Rebuild/replay the K718 port with recorded provenance; never flashes implicitly."""
import argparse
import datetime
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import uuid
from k718_lock import digest, hydrate, verify_libraries, prepare_libraries
from k718_web import gzip_assets
from k718_repro import prepare_tool, compare_releases

ROOT = Path(__file__).resolve().parents[1]

def config(): return json.loads((ROOT / 'port/config.json').read_text())
def write_json(path, data):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n')
def git(*args, root=None):
    return subprocess.check_output(['git', '-C', str(root or ROOT), *args], text=True).strip()
def protocol(root=None):
    text = ((root or ROOT) / 'lib/NanoPbComm/src/Protocol.h').read_text()
    match = re.search(r'PROTOCOL_VERSION\s*=\s*(\d+)', text)
    if not match: raise RuntimeError('Protocol declaration changed; review compatibility manually')
    return int(match[1])
def require_clean():
    if git('status', '--porcelain'): raise RuntimeError('Tracked or untracked source changes must be committed before export/build')
def guard_source():
    cfg = config()
    if protocol() != cfg['controller_protocol']:
        raise RuntimeError('Controller protocol changed; do not renumber it to bypass compatibility review')
    subprocess.run(['git','-C',str(ROOT),'merge-base','--is-ancestor',cfg['upstream']['commit'],'HEAD'],check=True)
    replay = ROOT / '.port/replay.json'
    if replay.exists():
        data = json.loads(replay.read_text())
        if data['target_commit'] != cfg['upstream']['commit']:
            raise RuntimeError('New upstream is unadopted. Review the report, resolve/adapt, then bind-upstream')
        if data.get('state') != 'complete': raise RuntimeError('Replay is not complete')
    return cfg
def tool_env():
    env = dict(os.environ)
    tools = Path(env.get('K718_TOOLS', ROOT / '.tools'))
    tools = tools.resolve()
    env['K718_TOOLS'] = str(tools)
    env.pop('K718_REPRO_MKLITTLEFS', None)
    paths = [p for p in (tools/'bin',tools/'node/bin') if p.exists()]
    env['PATH'] = os.pathsep.join([str(p) for p in paths] + [env.get('PATH','')])
    env['PLATFORMIO_CORE_DIR'] = env.get('PLATFORMIO_CORE_DIR',str(tools/'pio-core'))
    env['PLATFORMIO_SETTING_ENABLE_TELEMETRY'] = 'no'
    env['PLATFORMIO_SETTING_CHECK_PLATFORMIO_INTERVAL'] = '0'
    env['SOURCE_DATE_EPOCH'] = git('show','-s','--format=%at','HEAD')
    env['CI'] = 'true'
    env['TZ'] = 'UTC'
    env['LC_ALL'] = 'C'
    env['PYTHONHASHSEED'] = '0'
    return env

class Audit:
    def __init__(self, name):
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
        self.path = ROOT / 'dist/audit' / (stamp+'-'+name+'-'+uuid.uuid4().hex[:8])
        self.path.mkdir(parents=True)
        self.data = {'command':name,'source_commit':git('rev-parse','HEAD'),'source_tree':git('rev-parse','HEAD^{tree}'),'steps':[]}
        self.save()
    def save(self): write_json(self.path/'report.json',self.data)
    def run(self, label, argv, cwd=None):
        log = self.path/(label+'.log')
        print(label,flush=True)
        with log.open('w') as out:
            result = subprocess.run([str(x) for x in argv],cwd=cwd or ROOT,env=tool_env(),stdout=out,stderr=subprocess.STDOUT)
        self.data['steps'].append({'step':label,'exit_code':result.returncode,'log':log.name,'sha256':digest(log)})
        self.save()
        if result.returncode: raise RuntimeError(label+' failed; see '+str(log))
    def complete(self): self.data['passed']=True; self.save(); return self.path

def doctor():
    env=tool_env();cfg=config();observed={}
    for key, argv in [('platformio',[sys.executable,'-m','platformio','--version']),('node',['node','--version']),('npm',['npm','--version'])]:
        value=subprocess.check_output(argv,env=env,text=True).strip()
        observed[key]=value.split('version ')[-1] if key=='platformio' else value
    observed['pillow']=importlib.metadata.version('Pillow')
    for name,value in observed.items():
        if value != cfg['tools'][name]: raise RuntimeError(f'Tool {name}: expected {cfg["tools"][name]}, found {value}')
    lock=json.loads((ROOT/'port/dependencies.lock.json').read_text())
    core=Path(env['PLATFORMIO_CORE_DIR'])
    for name,entry in lock['packages'].items():
        p=core/'packages'/name/'package.json'
        if not p.exists(): continue # pio pkg install will install an absent package
        if json.loads(p.read_text())['version']!=entry['version'] or digest(p)!=entry['manifest_sha256']:
            raise RuntimeError('Toolchain package differs from lock: '+name)
    filesystem = prepare_tool(ROOT, Path(env['K718_TOOLS']))
    return {'tools':observed,'python':sys.version.split()[0],'core_dir':str(core),'reproducible_filesystem':filesystem}

def deps(audit, cache=None):
    lock=json.loads((ROOT/'port/dependencies.lock.json').read_text())
    if cache: hydrate(ROOT,Path(cache).resolve(),lock)
    audit.run('dependencies',[sys.executable,'-m','platformio','pkg','install','-e','display-knob'])
    prepare_libraries(ROOT,lock)
    verify_libraries(ROOT,lock)

def check_generated():
    expected=json.loads((ROOT/'port/generated-assets.json').read_text())
    for name,value in expected.items():
        if digest(ROOT/name)!=value: raise RuntimeError('Stale battery asset/input: '+name+'; run assets and review generated diffs')

def checks(audit):
    guard_source(); check_generated()
    audit.run('publication-boundary',[sys.executable,'scripts/check_public.py'])
    audit.run('workflow-failure-cases',[sys.executable,'-m','unittest','discover','-s','test/knob_workflow'])
    audit.run('native-ui',[sys.executable,'test/knob_native/check_ui.py'])
    audit.run('diagnostic-nonblocking',[sys.executable,'test/knob_native/check_diagnostics.py'])
    for name,source in [('geometry','test/knob_geometry/check.cpp'),('brightness','test/knob_native/check_brightness.cpp'),('hardware-policy','test/knob_native/check_hardware.cpp'),('led-gauges','test/knob_native/check_led_gauges.cpp'),('led-wire','test/knob_native/check_led_encoding.cpp'),('rotary-standby','test/knob_native/check_rotary_policy.cpp')]:
        binary=audit.path/name
        audit.run(name+'-compile',['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-Isrc',source,'-o',binary])
        audit.run(name,[binary])
    audit.run('theme-preview',[sys.executable,'test/knob_native/render_preview.py'])

def export_stack(destination):
    require_clean();cfg=guard_source();destination=Path(destination).resolve()
    if destination.exists(): raise RuntimeError('Stack destination must be new: '+str(destination))
    destination.mkdir(parents=True)
    commits=git('rev-list','--reverse',cfg['upstream']['commit']+'..HEAD').splitlines()
    if not commits: raise RuntimeError('No port commits to export')
    patches=[]
    for number,commit in enumerate(commits,1):
        name=f'{number:04}.patch';path=destination/name
        path.write_bytes(subprocess.check_output(['git','-C',str(ROOT),'format-patch','--stdout','--binary','--full-index','--no-signature','-1',commit]))
        patches.append({'file':name,'sha256':digest(path),'commit':commit,'subject':git('show','-s','--format=%s',commit)})
    write_json(destination/'series.json',{'schema':1,'upstream':cfg['upstream'],'source_commit':git('rev-parse','HEAD'),'source_tree':git('rev-parse','HEAD^{tree}'),'patches':patches,'note':'The complete ordered stack, not intermediate commits, is the build/release unit.'})
    return destination

def checked_stack(folder):
    folder=Path(folder).resolve();data=json.loads((folder/'series.json').read_text())
    if data.get('schema')!=1 or not data.get('patches'): raise RuntimeError('Invalid stack manifest')
    for p in data['patches']:
        path=(folder/p['file']).resolve()
        if path.parent!=folder or digest(path)!=p['sha256']: raise RuntimeError('Patch checksum/path mismatch: '+p['file'])
    return data

def replay(stack,destination,upstream=None,fetch_from=None):
    stack=Path(stack).resolve();data=checked_stack(stack);dest=Path(destination).resolve()
    if dest.exists(): raise RuntimeError('Replay destination must be a new directory')
    target=upstream or data['upstream']['commit']
    subprocess.run(['git','clone','--no-hardlinks','--no-checkout',str(ROOT),str(dest)],check=True,stdout=subprocess.DEVNULL)
    try: target_sha=git('rev-parse','--verify',target+'^{commit}',root=dest)
    except subprocess.CalledProcessError:
        subprocess.run(['git','-C',str(dest),'fetch','--no-tags',fetch_from or data['upstream']['url'],target],check=True)
        target_sha=git('rev-parse','FETCH_HEAD',root=dest)
    subprocess.run(['git','-C',str(dest),'checkout','--detach',target_sha],check=True,stdout=subprocess.DEVNULL)
    state={'schema':1,'target_commit':target_sha,'upstream_ref':target,'fetch_source':fetch_from or data['upstream']['url'],'old_upstream':data['upstream'],'expected_patches':len(data['patches']),'state':'applying','patches':data['patches']}
    report=dest/'.port/replay.json';write_json(report,state)
    overlap=set(git('diff','--name-only',data['upstream']['commit'],target_sha,root=dest).splitlines())
    state['upstream_changed_paths']=sorted(overlap)
    with (report.parent/'git-am.log').open('w') as log:
        result=subprocess.run(['git','-C',str(dest),'-c','user.name=GaggiMate Knob Screen','-c','user.email=maintainer@localhost.invalid','-c','commit.gpgsign=false','am','--keep-cr','--3way','--committer-date-is-author-date',*[str(stack/p['file']) for p in data['patches']]],stdout=log,stderr=subprocess.STDOUT)
    if result.returncode:
        state['state']='conflict';state['unmerged']=git('diff','--name-only','--diff-filter=U',root=dest).splitlines();write_json(report,state)
        raise RuntimeError('Replay stopped with reviewable conflict state at '+str(dest)+'; see .port/replay.json and git-am.log')
    state['state']='complete';state['result_commit']=git('rev-parse','HEAD',root=dest);state['result_tree']=git('rev-parse','HEAD^{tree}',root=dest)
    if target_sha==data['upstream']['commit'] and state['result_tree']!=data['source_tree']:
        state['state']='tree-mismatch';write_json(report,state);raise RuntimeError('Same-base replay did not reconstruct the expected source tree')
    state['source_tree_matches_export']=state['result_tree']==data['source_tree'];write_json(report,state)
    return state

def bind_upstream(version, expected_protocol):
    report=ROOT/'.port/replay.json';state=json.loads(report.read_text())
    if (ROOT/'.git/rebase-apply').exists(): raise RuntimeError('Finish git am conflict resolution first')
    count=int(git('rev-list','--count',state['target_commit']+'..HEAD'))
    if count!=state['expected_patches']:raise RuntimeError('A patch was skipped or history changed; review/re-export the stack instead of silently accepting it')
    if protocol()!=expected_protocol:raise RuntimeError('Declared controller protocol does not match source')
    cfg=config();cfg['upstream'].update(commit=state['target_commit'],ref=state['upstream_ref'],version=version)
    cfg['controller_protocol']=expected_protocol;cfg['tested_controller']='Pending validation for this upstream'
    write_json(ROOT/'port/config.json',cfg)
    state['state']='complete';state['manual_upstream_binding']=True;write_json(report,state)
    print('Binding recorded. Review generated UI/contracts/dependencies, commit the adaptations, then check/build. No hardware compatibility is implied.')

def web_bundle(audit):
    audit.run('npm-ci',['npm','ci'],ROOT/'web')
    audit.run('web-build',['npm','run','build'],ROOT/'web')
    dist=ROOT/'web/dist'
    gzip_assets(dist)
    audit.run('web-embed',[sys.executable,'scripts/embed_webui.py','--src',dist,'--out',ROOT/'src/display/webassets'])
    audit.run('web-encoding',[sys.executable,'scripts/k718_web.py','--validate',ROOT/'src/display/webassets'])
    header=(ROOT/'src/display/webassets/web_ui_manifest.h').read_text()
    count=re.search(r'WEB_ASSETS_COUNT\s*=\s*(\d+)',header)
    if not count or int(count[1])<1:raise RuntimeError('Empty embedded web bundle')

def partition_entries(data):
    result={}
    for offset in range(0,len(data),32):
        entry=data[offset:offset+32]
        if len(entry)<32 or entry[:2]!=b'\xaaP':break
        _,kind,subtype,address,size,label,_=struct.unpack('<HBBII16sI',entry)
        result[label.split(b'\0',1)[0].decode()]={'type':kind,'subtype':subtype,'offset':address,'size':size}
    return result

def build(destination=None,cache=None):
    require_clean();cfg=guard_source();environment=doctor();audit=Audit('build')
    output=Path(destination).resolve() if destination else ROOT/'dist/releases'/git('rev-parse','HEAD^{tree}')[:12]
    if output.exists():raise RuntimeError('Release output already exists; choose a new --out to retain evidence')
    deps(audit,cache);environment=doctor();checks(audit);web_bundle(audit)
    audit.run('firmware',[sys.executable,'-m','platformio','run','-e','display-knob'])
    # PlatformIO may clean the build directory when switching to buildfs.
    # Capture application outputs immediately; never rely on old leftovers.
    staged=audit.path/'staging'/'display-knob';staged.mkdir(parents=True)
    for name in ('firmware.bin','firmware.elf','bootloader.bin','partitions.bin'):
        shutil.copyfile(ROOT/'.pio/build/display-knob'/name,staged/name)
    audit.run('filesystem',[sys.executable,'-m','platformio','run','-e','display-knob','-t','buildfs'])
    shutil.copyfile(ROOT/'.pio/build/display-knob/littlefs.bin',staged/'littlefs.bin')
    audit.run('filesystem-metadata',[sys.executable,'scripts/k718_repro.py','--verify-filesystem',staged/'littlefs.bin'])
    audit.run('package',[sys.executable,'scripts/package_knob.py','--target','display-knob','--build-root',staged.parent])
    source=ROOT/'dist/k718/display-knob';parts=partition_entries((source/'partitions.bin').read_bytes())
    if parts.get('app0',{}).get('offset')!=cfg['application_offset'] or parts['app0']['size']!=cfg['application_size']:
        raise RuntimeError('Application partition layout changed; explicit migration required')
    if parts.get('spiffs',{}).get('offset')!=cfg['filesystem_offset']:raise RuntimeError('Filesystem layout changed')
    if (source/'firmware.bin').stat().st_size>cfg['application_size']:raise RuntimeError('Firmware does not fit app0')
    shutil.copytree(source,output)
    core=Path(tool_env()['PLATFORMIO_CORE_DIR'])
    shutil.copyfile(core/'packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin',output/'boot_app0.bin')
    export_stack(output/'source-stack')
    audit.complete()
    (output/'audit').mkdir()
    for log in audit.path.iterdir():
        if log.is_file() and (log.suffix=='.log' or log.name=='report.json'):
            shutil.copyfile(log,output/'audit'/log.name)
    for preview in (ROOT/'dist/preview').rglob('*.png'):
        target=output/'preview'/preview.relative_to(ROOT/'dist/preview')
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(preview,target)
    manifest={'schema':1,'qualification':'software-verified','hardware_verified':False,'upstream':cfg['upstream'],'port_version':cfg['port_version'],'release_channel':cfg.get('release_channel','development'),'source_commit':git('rev-parse','HEAD'),'source_tree':git('rev-parse','HEAD^{tree}'),'source_date_epoch':tool_env()['SOURCE_DATE_EPOCH'],'target':cfg['target'],'chip':cfg['chip'],'required_hardware_checks':cfg['required_hardware_checks'],'controller_protocol':cfg['controller_protocol'],'application_offset':cfg['application_offset'],'application_size':cfg['application_size'],'environment':environment,'reproducibility':environment['reproducible_filesystem'],'dependency_lock_sha256':digest(ROOT/'port/dependencies.lock.json'),'npm_lock_sha256':digest(ROOT/'web/package-lock.json'),'audit':'audit/report.json','audit_report_sha256':digest(output/'audit/report.json'),'artifacts':{p.name:{'bytes':p.stat().st_size,'sha256':digest(p)} for p in sorted(output.iterdir()) if p.is_file() and p.suffix in {'.bin','.elf'}}}
    write_json(output/'release.json',manifest);print('Release:',output)
    return output

def assets():
    audit=Audit('assets');audit.run('asset-tools',['npm','ci'],ROOT/'port/tools')
    audit.run('battery-glyphs',['node','scripts/generate_knob_battery.mjs'])
    audit.run('native-ui-generate',[sys.executable,'scripts/generate_knob_ui.py'])
    paths=[ROOT/'scripts/generate_knob_battery.mjs',ROOT/'port/tools/package-lock.json',ROOT/'src/display/ui/default/KnobBatteryImages.c',ROOT/'src/display/ui/default/KnobBatteryImages.h',*sorted((ROOT/'assets/k718-battery').glob('*.svg')),*sorted((ROOT/'assets/k718-battery').glob('*.png'))]
    write_json(ROOT/'port/generated-assets.json',{str(p.relative_to(ROOT)):digest(p) for p in paths});audit.complete()
    print('Review and commit generated changes before building a release.')

def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    sub.add_parser('doctor');sub.add_parser('assets');sub.add_parser('check')
    p=sub.add_parser('verify-repro');p.add_argument('first');p.add_argument('second');p.add_argument('--out',required=True)
    p=sub.add_parser('deps');p.add_argument('--cache-from')
    p=sub.add_parser('build');p.add_argument('--out');p.add_argument('--cache-from')
    p=sub.add_parser('export');p.add_argument('--out',required=True)
    p=sub.add_parser('replay');p.add_argument('--stack',required=True);p.add_argument('--dest',required=True);p.add_argument('--upstream');p.add_argument('--fetch-from')
    p=sub.add_parser('bind-upstream');p.add_argument('--version',required=True);p.add_argument('--controller-protocol',type=int,required=True)
    args=parser.parse_args()
    try:
        if args.command=='doctor':print(json.dumps(doctor(),indent=2))
        elif args.command=='export':print(export_stack(args.out))
        elif args.command=='replay':print(json.dumps(replay(args.stack,args.dest,args.upstream,args.fetch_from),indent=2))
        elif args.command=='bind-upstream':bind_upstream(args.version,args.controller_protocol)
        elif args.command=='verify-repro':
            from k718_flash import verified_release
            target=Path(args.out)
            if target.exists():raise RuntimeError('Comparison output must be new')
            first,a,_=verified_release(args.first);second,b,_=verified_release(args.second)
            write_json(target,compare_releases(first,second,(a,b)));print(target)
        elif args.command=='assets':assets()
        elif args.command=='build':build(args.out,args.cache_from)
        else:
            doctor();audit=Audit(args.command)
            if args.command=='deps':deps(audit,args.cache_from)
            else:checks(audit)
            print(audit.complete())
    except (RuntimeError,subprocess.CalledProcessError) as error:
        print('STOP:',error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':sys.exit(main())
