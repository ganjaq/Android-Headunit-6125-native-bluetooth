import copy, json, sys, tempfile, unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import ngd_patch as ngd
import ngd_avrcp_release as kit


class AvrcpReleaseTests(unittest.TestCase):
    def setUp(self):
        self.reviewed = json.loads((ROOT/'profiles/payload-avrcp-20261019.json').read_text(encoding='utf-8'))

    def synthetic(self, root, manifest):
        # Explicit synthetic files: tests exercise policy, not OEM integrity.
        for name in manifest['files']:
            p = root/'payload'/name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(b'SYNTHETIC TEST ONLY')
        (root/'manifest.json').write_text(json.dumps(manifest))

    def test_exact_reviewed_variant_policy(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.synthetic(root, self.reviewed)
            with patch.object(ngd, 'digest', side_effect=lambda p: self.reviewed['files'][p.relative_to(root/'payload').as_posix()]):
                self.assertEqual(ngd.verify_release(root)['variant'], 'avrcp-legacy-20261019')

    def test_self_consistent_but_unreviewed_artifacts_rejected(self):
        for name in ('BluetoothNGD.apk', 'ngd16_native_bt', 'lib/arm64/libngd_avrcp_legacy.so'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                changed = copy.deepcopy(self.reviewed)
                changed['files'][name] = '0'*64
                self.synthetic(root, changed)
                with patch.object(ngd, 'digest', side_effect=lambda p: changed['files'][p.relative_to(root/'payload').as_posix()]):
                    with self.assertRaisesRegex(RuntimeError, 'differs from reviewed'):
                        ngd.verify_release(root)

    def test_missing_bridge_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.synthetic(root, self.reviewed)
            (root/'payload/lib/arm64/libngd_avrcp_legacy.so').unlink()
            with self.assertRaisesRegex(RuntimeError, 'inventory differs'):
                ngd.verify_release(root)

    def test_unknown_variant_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            changed = copy.deepcopy(self.reviewed)
            changed['variant'] = 'unreviewed'
            self.synthetic(root, changed)
            with patch.object(ngd, 'digest', side_effect=lambda p: changed['files'][p.relative_to(root/'payload').as_posix()]):
                with self.assertRaisesRegex(RuntimeError, 'unknown variant'):
                    ngd.verify_release(root)

    def test_assembly_refuses_wrong_artifact_before_creating_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            artifact = root/'wrong.apk'
            artifact.write_bytes(b'wrong')
            with patch.object(kit, 'verify_release', return_value={}):
                with self.assertRaisesRegex(RuntimeError, 'unreviewed artifact'):
                    kit.assemble(root, artifact, artifact, artifact, root/'out')
            self.assertFalse((root/'out').exists())
