import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from k718_lock import verify_libraries

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
