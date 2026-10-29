"""Exercise the real replay/export code, including deliberate failure cases."""
import contextlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

REPO=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(REPO/'scripts'))
import k718

class ReplayTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.home=Path(self.tmp.name);self.repo=self.home/'source';self.repo.mkdir()
        self.original=k718.ROOT;k718.ROOT=self.repo;self.addCleanup(setattr,k718,'ROOT',self.original)
        self.git('init','-q');self.git('config','user.name','Test');self.git('config','user.email','test@local.invalid');self.git('config','commit.gpgsign','false')
        self.put('lib/NanoPbComm/src/Protocol.h','constexpr int PROTOCOL_VERSION = 5;\n')
        self.put('shared.txt','official\n');self.commit('official');self.base=self.git('rev-parse','HEAD')
        self.put('.gitignore','/dist/\n/.port/\n')
        self.put('port/config.json',json.dumps({'controller_protocol':5,'upstream':{'commit':self.base,'ref':'v1','version':'v1','url':str(self.repo)}}))
        self.put('shared.txt','knob\n');self.commit('port');self.tree=self.git('rev-parse','HEAD^{tree}')
    def git(self,*args):return subprocess.check_output(['git','-C',str(self.repo),*args],text=True,stderr=subprocess.DEVNULL).strip()
    def put(self,name,text):
        p=self.repo/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
    def commit(self,name):self.git('add','.');self.git('commit','-qm',name)
    def export(self):return k718.export_stack(self.home/'stack')
    def test_same_base_reconstructs_exact_tree(self):
        state=k718.replay(self.export(),self.home/'replayed')
        self.assertTrue(state['source_tree_matches_export']);self.assertEqual(state['result_tree'],self.tree)
    def test_replay_preserves_vendor_crlf_bytes(self):
        path=self.repo/'vendor.cpp'
        path.write_bytes(b'// Original vendor comment\r\nint value;\r\n')
        self.commit('vendor input')
        path.write_bytes(b'// Adapted vendor comment\r\nint value;\r\n')
        self.commit('adapt vendor comment')
        expected=self.git('rev-parse','HEAD^{tree}')
        state=k718.replay(self.export(),self.home/'crlf-replay')
        self.assertEqual(state['result_tree'],expected)
        self.assertEqual((self.home/'crlf-replay/vendor.cpp').read_bytes(),path.read_bytes())
    def test_checksum_rejects_tampered_patch_before_clone(self):
        folder=self.export();(folder/'0001.patch').write_text('corrupt')
        with self.assertRaisesRegex(RuntimeError,'checksum'):k718.replay(folder,self.home/'bad')
        self.assertFalse((self.home/'bad').exists())
    def test_dirty_source_cannot_export(self):
        self.put('unrecorded.txt','oops')
        with self.assertRaisesRegex(RuntimeError,'committed'):self.export()
    def test_protocol_change_stops_before_build(self):
        self.put('lib/NanoPbComm/src/Protocol.h','constexpr int PROTOCOL_VERSION = 6;\n');self.commit('protocol')
        with self.assertRaisesRegex(RuntimeError,'protocol changed'):k718.guard_source()
    def test_changed_upstream_conflict_is_preserved(self):
        stack=self.export();self.git('checkout','-q','--detach',self.base)
        self.put('shared.txt','new official incompatible edit\n');self.commit('next official');new=self.git('rev-parse','HEAD')
        dest=self.home/'conflict'
        with self.assertRaisesRegex(RuntimeError,'conflict state'):k718.replay(stack,dest,new)
        state=json.loads((dest/'.port/replay.json').read_text())
        self.assertEqual(state['state'],'conflict');self.assertIn('shared.txt',state['unmerged'])
        self.assertTrue((dest/'.git/rebase-apply').is_dir())
    def test_nonconflicting_new_upstream_requires_binding(self):
        stack=self.export();self.git('checkout','-q','--detach',self.base)
        self.put('new-official.txt','upstream addition\n');self.commit('next official');new=self.git('rev-parse','HEAD')
        dest=self.home/'next';k718.replay(stack,dest,new)
        k718.ROOT=dest
        with self.assertRaisesRegex(RuntimeError,'unadopted'):k718.guard_source()
        k718.bind_upstream('v2',5)
        self.assertEqual(k718.guard_source()['upstream']['commit'],new)
    def test_partition_parser(self):
        import struct
        data=struct.pack('<HBBII16sI',0x50aa,0,0x10,0x10000,0x640000,b'app0',0)+b'\xff'*32
        self.assertEqual(k718.partition_entries(data)['app0']['offset'],0x10000)

if __name__=='__main__':unittest.main()
