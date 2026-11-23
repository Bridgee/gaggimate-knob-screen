"""Compile the replay transport with the actual generated schema and sanitizers."""
from pathlib import Path
import argparse, subprocess
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--environment',choices=['display-knob','display-knob-validation'],default='display-knob');a=p.parse_args()
out=ROOT/'dist/replay-check'/a.environment;out.mkdir(parents=True,exist_ok=True)
(out/'Arduino.h').write_text('#pragma once\n#include <cstdint>\n#include <cstddef>\n')
gen=ROOT/'.pio/build'/a.environment/'nanopb/generated-src';pb=ROOT/'.pio/libdeps'/a.environment/'Nanopb'
flags=['-g','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(out),'-I'+str(gen),'-I'+str(pb),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'lib/NanoPbComm/src')]
objects=[]
for source in [gen/'gaggimate.pb.c',*[pb/n for n in ['pb_common.c','pb_encode.c','pb_decode.c']]]:
    obj=out/(source.stem+'.o');subprocess.run(['clang','-std=c99',*flags,'-c',str(source),'-o',str(obj)],check=True);objects.append(str(obj))
binary=out/'replay'
subprocess.run(['clang++','-std=c++17',*flags,'-DGAGGIMATE_KNOB_VALIDATION',str(ROOT/'test/knob_native/check_replay.cpp'),str(ROOT/'src/display/validation/ReplayPeer.cpp'),*objects,'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True,timeout=20)
