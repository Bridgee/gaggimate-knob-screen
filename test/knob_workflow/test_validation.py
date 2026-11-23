import json,struct,sys,tempfile,unittest
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
                (root/'port/config.json').write_text((ROOT/'port/config.json').read_text())
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
