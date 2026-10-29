import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from k718 import write_json
from k718_lock import digest
from k718_flash import verified_release, qualification, flash

class ReleaseTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name)
        for name,data in [('firmware.bin',b'example app'),('boot_app0.bin',b'\xff'*8192),('partitions.bin',struct.pack('<HBBII16sI',0x50aa,0,0x10,0x10000,0x640000,b'app0',0))]:
            (self.root/name).write_bytes(data)
        self.manifest={'schema':1,'target':'display-knob','chip':'ESP32-S3','source_tree':'test-tree','controller_protocol':5,'required_hardware_checks':['display','touch'],'application_offset':0x10000,'application_size':0x640000,'artifacts':{p.name:{'sha256':digest(p),'bytes':p.stat().st_size} for p in self.root.glob('*.bin')}}
        self.save()
    def save(self):write_json(self.root/'release.json',self.manifest)
    def results(self,**overrides):
        result={'firmware_sha256':self.manifest['artifacts']['firmware.bin']['sha256'],'checks':{'display':True,'touch':True},'screen_mac':'screen','controller_identity':'bench controller','controller_protocol':5};result.update(overrides)
        write_json(self.root/'results.json',result);return self.root/'results.json'
    def test_release_checksums(self):verified_release(self.root)
    def test_changed_binary_rejected(self):
        (self.root/'firmware.bin').write_bytes(b'changed app')
        with self.assertRaisesRegex(RuntimeError,'checksum'):verified_release(self.root)
    def test_path_escape_rejected(self):
        self.manifest['artifacts']['../outside']={'bytes':0,'sha256':'bad'};self.save()
        with self.assertRaisesRegex(RuntimeError,'checksum/path'):verified_release(self.root)
    def test_incomplete_hardware_checks_rejected(self):
        with self.assertRaisesRegex(RuntimeError,'not all passed'):qualification(self.root,self.results(checks={'display':True}),self.root/'qualified.json')
    def test_wrong_tested_firmware_rejected(self):
        with self.assertRaisesRegex(RuntimeError,'SHA'):qualification(self.root,self.results(firmware_sha256='old-image'),self.root/'qualified.json')
    def test_qualification_bound_to_release(self):
        output=qualification(self.root,self.results(),self.root/'qualified.json')
        self.assertEqual(json.loads(output.read_text())['release_sha256'],digest(self.root/'release.json'))
    def test_unqualified_update_rejected_without_contacting_device(self):
        with self.assertRaisesRegex(RuntimeError,'qualification'):flash(self.root,None,None,None)

if __name__=='__main__':unittest.main()
