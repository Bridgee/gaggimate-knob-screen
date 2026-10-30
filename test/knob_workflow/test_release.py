import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import ModuleType, SimpleNamespace
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from k718 import write_json
from k718_lock import digest
from k718_flash import verified_release, qualification, flash
from k718_otadata import record

class ReleaseTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name)
        for name,data in [('firmware.bin',b'example app'),('boot_app0.bin',b'\xff'*8192),('partitions.bin',struct.pack('<HBBII16sI',0x50aa,0,0x10,0x10000,0x640000,b'app0',0)+struct.pack('<HBBII16sI',0x50aa,0,0x11,0x650000,0x640000,b'app1',0))]:
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
    def test_usb_retry_preserves_stub_for_postwrite_verification(self):
        # Match esptool 4.9's retry: mutate the supplied loader, while retaining
        # a newly wrapped stub only inside write_flash. No device is contacted.
        nvs=struct.pack('<HBBII16sI',0x50aa,1,2,0x9000,0x5000,b'nvs',0)
        ota=struct.pack('<HBBII16sI',0x50aa,1,0,0xe000,0x2000,b'otadata',0)
        (self.root/'partitions.bin').write_bytes((self.root/'partitions.bin').read_bytes()+nvs+ota)
        self.manifest['artifacts']['partitions.bin']={'sha256':digest(self.root/'partitions.bin'),'bytes':(self.root/'partitions.bin').stat().st_size};self.save()
        (self.root/'bootloader.bin').write_bytes(b'reviewed bootloader')
        self.manifest['artifacts']['bootloader.bin']={'sha256':digest(self.root/'bootloader.bin'),'bytes':(self.root/'bootloader.bin').stat().st_size};self.save()
        device=bytearray(b'\xff'*0xc90000)
        device[:len(b'reviewed bootloader')]=b'reviewed bootloader'
        device[0xe000:0xe020]=record(1,2)
        table=(self.root/'partitions.bin').read_bytes();device[0x8000:0x8000+len(table)]=table
        class Port:
            def setDTR(self,value):pass
            def close(self):pass
        class ROM:
            IS_STUB=False
            def __init__(self,*args):self._port=Port()
            def connect(self,*args,**kwargs):pass
            def read_mac(self):return bytes(range(6))
            def run_stub(self):return Stub(self)
            def change_baud(self,*args):pass
            def flash_set_parameters(self,*args):pass
            def flash_md5sum(self,offset,size):return hashlib.md5(device[offset:offset+size]).hexdigest()
            def read_flash(self,offset,size):
                if not self.IS_STUB:raise RuntimeError('FF00: Command not implemented')
                return bytes(device[offset:offset+size])
            def hard_reset(self):pass
        class Stub(ROM):
            IS_STUB=True
            def __init__(self,loader):self._port=loader._port
        ROM.STUB_CLASS=Stub
        def retry_write(loader,args):
            loader.IS_STUB=False
            offset,stream=args.addr_filename[0];data=stream.read();device[offset:offset+len(data)]=data
        target=ModuleType('esptool.targets.esp32s3');target.ESP32S3ROM=ROM
        commands=ModuleType('esptool.cmds');commands.write_flash=retry_write
        modules={'esptool':ModuleType('esptool'),'esptool.targets':ModuleType('esptool.targets'),'esptool.targets.esp32s3':target,'esptool.cmds':commands}
        backup=self.root/'backup'
        with patch.dict(sys.modules,modules),patch('k718_flash.tool_env',return_value={'PLATFORMIO_CORE_DIR':str(self.root)}):
            flash(self.root,'fake-port',':'.join(f'{x:02x}' for x in range(6)),backup,validation=True)
        receipt=json.loads((backup/'flash.json').read_text())
        self.assertTrue(receipt['written']);self.assertTrue(receipt['readback_verified'])
        self.assertEqual(receipt['scope'],'app1 + explicit boot selection')
        self.assertEqual(bytes(device[0xe000:0xf000]),(backup/'otadata.bin').read_bytes()[:4096])
        self.assertEqual(bytes(device[0xf000:0xf020]),record(2,0))
        for name in ('partition-table','nvs','app0'):
            region=receipt['backups'][name]
            self.assertEqual(bytes(device[region['offset']:region['offset']+region['bytes']]),(backup/(name+'.bin')).read_bytes())

    def test_unqualified_update_rejected_without_contacting_device(self):
        with self.assertRaisesRegex(RuntimeError,'qualification'):flash(self.root,None,None,None)

if __name__=='__main__':unittest.main()
