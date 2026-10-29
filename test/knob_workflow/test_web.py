import gzip
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from embed_webui import pack
from k718_web import gzip_assets, validate_web_assets


class WebBundleTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source = self.root / 'dist'
        (self.source / 'assets').mkdir(parents=True)
        (self.source / 'index.html').write_text('<!doctype html><script type="module" src="/assets/app.js"></script>')
        (self.source / 'assets/app.js').write_text('console.log("real application");')
        self.output = self.root / 'embedded'

    def verify(self):
        pack(str(self.source), str(self.output))
        return validate_web_assets(self.output)

    def test_compression_suffix_gives_correct_http_encoding_and_logical_urls(self):
        gzip_assets(self.source)
        self.assertFalse((self.source / 'index.html').exists())
        self.assertTrue((self.source / 'index.html.gz').exists())
        result = self.verify()
        self.assertEqual(result['gzip_assets'], 2)
        self.assertEqual(result['index_references'], ['/assets/app.js'])

    def test_actual_regression_gzipped_payload_without_suffix_is_rejected(self):
        path = self.source / 'index.html'
        path.write_bytes(gzip.compress(path.read_bytes()))
        with self.assertRaisesRegex(RuntimeError, 'Gzip flag/payload mismatch'):
            self.verify()

    def test_uncompressed_valid_bundle_supported(self):
        self.assertEqual(self.verify()['gzip_assets'], 0)

    def test_double_compression_rejected(self):
        gzip_assets(self.source)
        path = self.source / 'index.html.gz'
        path.write_bytes(gzip.compress(path.read_bytes()))
        with self.assertRaisesRegex(RuntimeError, 'Double gzip'):
            self.verify()

    def test_corrupt_compressed_payload_rejected(self):
        gzip_assets(self.source)
        (self.source / 'index.html.gz').write_bytes(b'\x1f\x8b broken stream')
        with self.assertRaisesRegex(RuntimeError, 'Corrupt gzip'):
            self.verify()

    def test_missing_javascript_entry_rejected(self):
        (self.source / 'assets/app.js').unlink()
        with self.assertRaisesRegex(RuntimeError, 'Missing index web references'):
            self.verify()

    def test_compression_rejects_already_compressed_input(self):
        path = self.source / 'index.html'
        path.write_bytes(gzip.compress(path.read_bytes()))
        with self.assertRaisesRegex(RuntimeError, 'Already compressed'):
            gzip_assets(self.source)
