import json,struct,subprocess,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
from k718_validation_source import production_source
from k718_flash import verified_release
from k718_lock import digest

class ValidationIsolationTest(unittest.TestCase):
    def test_production_projection_keeps_code_and_nested_directives(self):
        source=b'normal();\n#ifdef GAGGIMATE_KNOB_VALIDATION\nblocked();\n#else\n#if OTHER\noriginal();\n#endif\n#endif\nend();\n'
        self.assertEqual(production_source(source),b'normal();\n#if OTHER\noriginal();\n#endif\nend();')
        self.assertNotEqual(production_source(source),production_source(source.replace(b'normal();',b'changed();')))
        with self.assertRaises(RuntimeError):production_source(b'#ifdef GAGGIMATE_KNOB_VALIDATION\n')
    def test_validation_image_cannot_be_relabelled_as_production(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            data={'firmware.bin':b'image\x91K718-VALIDATION-ONLY\x92','boot_app0.bin':bytes(8192),
                'partitions.bin':struct.pack('<HBBII16sI',0x50aa,0,0x10,0x10000,0x640000,b'app0',0)}
            for name,value in data.items():(root/name).write_bytes(value)
            manifest=dict(schema=1,target='display-knob-validation',chip='ESP32-S3',validation_only=True,
                application_offset=0x10000,application_size=0x640000,artifacts={n:{'bytes':len(v),'sha256':digest(root/n)} for n,v in data.items()})
            def save():(root/'release.json').write_text(json.dumps(manifest))
            save()
            with self.assertRaisesRegex(RuntimeError,'Unsupported'):verified_release(root)
            verified_release(root,allow_test_image=True)
            manifest['target']='display-knob';save()
            with self.assertRaisesRegex(RuntimeError,'Validation image'):verified_release(root)
            manifest['validation_only']=False;save()
            with self.assertRaisesRegex(RuntimeError,'Validation image'):verified_release(root)
            manifest['target']='display-knob-validation';manifest['validation_only']=True
            (root/'firmware.bin').write_bytes(b'normal image');manifest['artifacts']['firmware.bin']={'bytes':12,'sha256':digest(root/'firmware.bin')};save()
            with self.assertRaisesRegex(RuntimeError,'identity'):verified_release(root,allow_test_image=True)
    def test_generated_permissions_follow_explicit_build_target(self):
        for target in ['display-knob','display-knob-validation']:
            with tempfile.TemporaryDirectory() as folder:
                root=Path(folder);(root/'src').mkdir();(root/'port').mkdir();(root/'scripts').mkdir()
                config=json.loads((ROOT/'port/config.json').read_text());config.update(release_channel='candidate',port_version='0.5.0-rc.12')
                (root/'port/config.json').write_text(json.dumps(config))
                (root/'scripts/k718_ota_config.py').write_text((ROOT/'scripts/k718_ota_config.py').read_text())
                class Env:
                    def subst(self,value):return target if value=='$PIOENV' else str(root)
                namespace={'Import':lambda name:namespace.update(env=Env()),'__file__':str(ROOT/'scripts/knob_firmware_version.py')}
                with patch('subprocess.check_output',side_effect=['12345678abcdef','','1700000000']):
                    exec(compile((ROOT/'scripts/knob_firmware_version.py').read_text(),'version','exec'),namespace)
                header=(root/'src/version.h').read_text()
                self.assertIn('#define BUILD_SCREEN_OTA_STANDALONE_QUALIFICATION '+str(int(target.endswith('validation'))),header)
                self.assertIn('#define BUILD_SCREEN_OTA_QUALIFICATION 0',header)
                if target.endswith('validation'):
                    config=json.loads((root/'port/config.json').read_text());config.update(release_channel='stable',port_version='0.5.0');(root/'port/config.json').write_text(json.dumps(config))
                    with self.assertRaisesRegex(RuntimeError,'candidate'):exec(compile((ROOT/'scripts/knob_firmware_version.py').read_text(),'version','exec'),namespace)

    def test_profile_image_directory_is_compile_time_isolated(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            (root/'Arduino.h').write_text('#pragma once\n#include <cstdint>\n#include <cstddef>\n#include <cctype>\n#include <string>\nclass String: public std::string {\n public: using std::string::string;\n String(std::string value):std::string(std::move(value)) {}\n bool isEmpty() const { return empty(); }\n char charAt(size_t i) const { return at(i); }\n};\n')
            source=root/'test.cpp'
            source.write_text('#include <display/core/ProfileImage.h>\n#include <cassert>\nint main(){assert(profile_image::path("sample") == EXPECTED_PATH);assert(!profile_image::isValidId("../sample"));}\n')
            for validation in [False,True]:
                binary=root/('test' if validation else 'normal')
                flags=['-DGAGGIMATE_KNOB_VALIDATION'] if validation else []
                expected='/validation/pi/sample.bin' if validation else '/pi/sample.bin'
                subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(ROOT/'src'),*flags,'-DEXPECTED_PATH="'+expected+'"',str(source),'-o',str(binary)],check=True)
                subprocess.run([str(binary)],check=True)

    def test_actual_ready_callback_never_parses_simulated_controller_as_ota_version(self):
        source=(ROOT/'src/display/plugins/WebUIPlugin.cpp').read_text()
        body=source.split('pluginManager->on("controller:ready", [this](Event const &) {',1)[1].split('\n    });',1)[0]
        worker=(ROOT/'src/display/plugins/OfficialRelease.cpp').read_text().split('void Discovery::request(const std::string &channel, const std::string &controllerVersion) {',1)[1].split('\n}\nvoid Discovery::check()',1)[0]
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            (root/'Arduino.h').write_text('''#pragma once
#include <string>
#include <vector>
#include <cstdlib>
class String: public std::string {
 public: using std::string::string;
 String(std::string value):std::string(std::move(value)) {}
 String(int value):std::string(std::to_string(value)) {}
};
''')
            harness='''#include <Arduino.h>
#include <cassert>
#include <stdexcept>
#include "semver_extensions.h"
#include <functional>
#include <display/plugins/OfficialReleasePolicy.h>
namespace network_work { enum class Kind { Official };inline bool post(Kind,std::function<void()>){return true;} }
namespace official_release { struct Discovery {
 Mailbox mailbox;void check(){}
 void request(const std::string &channel,const std::string &controllerVersion){WORKERBODY}
}; }
struct Ota {
 int sets=0, inits=0;
 void setControllerVersion(const String &s) { ++sets;auto v=from_string(s);semver_free(&v); }
 void init() { ++inits; }
};
struct Controller { struct Info { String version; } info; const Info &getSystemInfo(){return info;} };
struct Bench { Ota *ota;Controller *controller;bool officialRefreshRequested=false;
 void ready(){BODY}
};
int main(){
 bool invalid=false;try{auto v=from_string("simulated");semver_free(&v);}catch(const std::out_of_range &){invalid=true;}
 assert(invalid); // The unchanged legacy parser rejects a synthetic identity.
 Ota ota;Controller controller;
#ifdef GAGGIMATE_KNOB_VALIDATION
 controller.info.version="simulated";
#else
 controller.info.version="1.10.0";
#endif
 Bench bench{&ota,&controller};bench.ready();assert(bench.officialRefreshRequested);
 official_release::Discovery discovery;discovery.request("latest",controller.info.version);official_release::Request request;assert(discovery.mailbox.take(request));
#ifdef GAGGIMATE_KNOB_VALIDATION
 assert(ota.sets==0 && ota.inits==0);assert(request.controllerVersion.empty());
#else
 assert(ota.sets==1 && ota.inits==1);assert(request.controllerVersion=="1.10.0");
#endif
}
'''.replace('WORKERBODY',worker+'\n').replace('BODY',body+'\n')
            (root/'ready.cpp').write_text(harness)
            obj=root/'semver.o'
            subprocess.run(['clang','-fsanitize=address,undefined','-c',str(ROOT/'lib/OTA/src/semver.c'),'-o',str(obj)],check=True)
            for validation in [False,True]:
                flags=['-DGAGGIMATE_KNOB_VALIDATION'] if validation else []
                binary=root/('validation' if validation else 'normal')
                subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-DGAGGIMATE_KNOB',*flags,'-I'+str(root),'-I'+str(ROOT/'lib/OTA/src'),'-I'+str(ROOT/'src'),str(root/'ready.cpp'),str(ROOT/'lib/OTA/src/semver_extensions.cpp'),str(obj),'-o',str(binary)],check=True)
                subprocess.run([str(binary)],check=True)
