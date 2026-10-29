import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from k718_lock import verify_libraries, prepare_libraries, library_identity, digest

class LocalDependencyTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name)
        self.folder=self.root/'.pio/libdeps/display-knob';self.folder.mkdir(parents=True)
        (self.root/'lib/NayrodPID').mkdir(parents=True);self.lock={'libraries':{}}
    def link(self,cwd):
        (self.folder/'NayrodPID.pio-link').write_text(json.dumps({'cwd':str(cwd),'spec':{'uri':'symlink://lib/NayrodPID'}}))
    def test_cache_can_be_verified_before_local_dependency_is_installed(self):verify_libraries(self.root,self.lock,require_local=False)
    def test_installed_dependencies_require_local_source_link(self):
        with self.assertRaisesRegex(RuntimeError,'Missing'):verify_libraries(self.root,self.lock)
    def test_project_manifest_satisfies_local_dependency_directly(self):
        (self.root/'lib/NayrodPID/library.json').write_text(json.dumps({'name':'NayrodPID'}))
        verify_libraries(self.root,self.lock)
    def test_local_link_must_reference_this_checkout(self):
        self.link(self.root);verify_libraries(self.root,self.lock)
        self.link(self.root/'another-checkout')
        with self.assertRaisesRegex(RuntimeError,'outside'):verify_libraries(self.root,self.lock)

if __name__=='__main__':unittest.main()


class PreparedDependencyTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.library=self.root/'.pio/libdeps/display-knob/Fixture';self.library.mkdir(parents=True)
        (self.library/'.piopm').write_text(json.dumps({'name':'Fixture','version':'1.0','spec':{'owner':'test'}}))
        (self.library/'source.h').write_text('unchanged source')
        pristine=library_identity(self.library)
        self.input=self.root/'port/descriptor.py';self.input.parent.mkdir();self.input.write_text('locked generated input')
        (self.library/'descriptor.py').write_bytes(self.input.read_bytes())
        prepared=library_identity(self.library)
        (self.library/'descriptor.py').unlink()
        self.lock={'libraries':{'Fixture':prepared},'prepared_libraries':{'Fixture':{'pristine_source_sha256':pristine['source_sha256'],'inputs':[{'source':'port/descriptor.py','target':'descriptor.py','sha256':digest(self.input)}]}}}
    def test_pristine_and_prepared_installs_have_identical_locked_identity(self):
        prepare_libraries(self.root,self.lock)
        self.assertEqual(library_identity(self.library),self.lock['libraries']['Fixture'])
        prepare_libraries(self.root,self.lock)
        self.assertEqual(library_identity(self.library),self.lock['libraries']['Fixture'])
    def test_unrelated_source_tampering_is_not_normalized_away(self):
        (self.library/'source.h').write_text('changed')
        with self.assertRaisesRegex(RuntimeError,'before preparation'):prepare_libraries(self.root,self.lock)
        self.assertFalse((self.library/'descriptor.py').exists())
    def test_changed_pinned_input_is_rejected_even_after_preparation(self):
        prepare_libraries(self.root,self.lock)
        self.input.write_text('changed')
        with self.assertRaisesRegex(RuntimeError,'input mismatch'):prepare_libraries(self.root,self.lock)
