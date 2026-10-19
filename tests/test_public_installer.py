import copy
import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import ngd_public_installer as public


class PublicInstallerTests(unittest.TestCase):
    def test_transport_handles_vendor_question_mark_serial(self):
        self.assertEqual(public.select_transport('List of devices attached\n? device product:trinket transport_id:4\n'), '4')

    def test_ambiguous_offline_unauthorized_and_old_adb_rejected(self):
        for output in ('List of devices attached\n',
                       'List of devices attached\na device transport_id:1\nb offline transport_id:2',
                       'List of devices attached\na unauthorized transport_id:1',
                       'List of devices attached\na device'):
            with self.subTest(output=output), self.assertRaises(RuntimeError):
                public.select_transport(output)

    def test_transport_change_never_retargets_command(self):
        adb = public.TransportAdb('adb', '4')
        with patch.object(public, 'run_adb', return_value='List of devices attached\n? device transport_id:5') as run:
            with self.assertRaisesRegex(RuntimeError, 'connection changed'):
                adb.sh('id -u')
            self.assertEqual(run.call_count, 1)

    def test_no_check_or_no_confirmation_never_contacts_device(self):
        engine = public.Installer('adb', 'missing', 'backups', root=False)
        with patch.object(public, 'connect') as connect:
            with self.assertRaises(RuntimeError): engine.install(accepted=True)
            engine.checked = {'synthetic': True}
            with self.assertRaises(RuntimeError): engine.install(accepted=False)
            with self.assertRaises(RuntimeError): engine.rollback(accepted=False)
            with self.assertRaises(RuntimeError): engine.restart(accepted=False)
            connect.assert_not_called()

    def test_existing_install_is_reported_without_payload_verification_or_writes(self):
        engine = public.Installer('adb', 'missing', 'backups', root=False)
        adb = Mock()
        adb.exists.return_value = True
        with patch.object(public, 'connect', return_value=adb), patch.object(public, 'device_identity', return_value='f'*64), \
                patch.object(public.ngd, 'verify_release') as verify:
            result = engine.check()
            self.assertEqual(result['status'], 'EXISTING_INSTALLATION')
            self.assertIsNone(engine.checked)
            verify.assert_not_called()
            adb.run.assert_not_called()

    def test_changed_state_invalidates_confirmation_before_installer_write(self):
        engine = public.Installer('adb', 'missing', 'backups', root=False)
        baseline = {'profile': 'synthetic', 'boot': 'old', 'audio': {'test': '0'}}
        changed = copy.deepcopy(baseline)
        changed['boot'] = 'new'
        engine.checked = {'identity': 'device', 'state': public.state_hash(baseline), 'manifest': 'kit'}
        with patch.object(public, 'connect'), patch.object(public, 'device_identity', return_value='device'), \
                patch.object(public.ngd, 'verify_release'), patch.object(public.ngd, 'inspect', return_value=changed), \
                patch.object(public.ngd, 'digest', return_value='kit'), patch.object(public.ngd, 'install') as install:
            with self.assertRaisesRegex(RuntimeError, 'state or kit changed'): engine.install(accepted=True)
            install.assert_not_called()
            self.assertIsNone(engine.checked)

    def test_changed_identity_rejected_after_root_reconnect(self):
        before, after = Mock(), Mock()
        before.prop.side_effect = lambda k: {'ro.board.platform':'trinket', 'ro.build.version.sdk':'29', 'ro.build.fingerprint':'same'}[k]
        before.sh.return_value = '2000'
        after.sh.return_value = '0'
        after.prop.return_value = 'same'
        with patch.object(public, 'run_adb', return_value='List of devices attached\n? device transport_id:4'), \
                patch.object(public, 'TransportAdb', side_effect=[before, after]), \
                patch.object(public, 'device_identity', side_effect=['first', 'other']), patch.object(public.time, 'sleep'):
            with self.assertRaisesRegex(RuntimeError, 'identity changed'): public.connect('adb', request_root=True)

    def test_reports_hash_identifiers_instead_of_copying_raw_values(self):
        adb = Mock()
        adb.prop.side_effect = ['SERIAL-PRIVATE', 'BOOT-PRIVATE']
        adb.sh.return_value = 'ANDROID-ID-PRIVATE'
        identity = public.device_identity(adb)
        self.assertEqual(len(identity), 64)
        self.assertNotIn('PRIVATE', identity)

    def test_baseline_rejects_self_consistent_unreviewed_settings(self):
        import json, tempfile
        ngd = public.ngd
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest = json.loads((ROOT/'profiles/payload-baseline-010.json').read_text(encoding='utf-8'))
            manifest['files']['Settings.bluetooth-visible.apk'] = '0'*64
            for name in manifest['files']:
                file = root/'payload'/name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(b'SYNTHETIC TEST ONLY')
            (root/'manifest.json').write_text(json.dumps(manifest))
            with patch.object(ngd, 'digest', side_effect=lambda path: manifest['files'][path.relative_to(root/'payload').as_posix()]):
                with self.assertRaisesRegex(RuntimeError, 'baseline payload differs'): ngd.verify_release(root)
