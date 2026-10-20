import hashlib
import json
import shutil
import struct
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import ngd_components as c
import ngd_patch as ngd
import build_component_bundle as exporter


class RecipeTests(unittest.TestCase):
    def test_persistent_adb_is_verified_and_modified_cache_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/'profiles').mkdir()
            (root/'platform-tools').mkdir()
            files = {}
            for name in ('adb.exe','AdbWinApi.dll','AdbWinUsbApi.dll'):
                data = ('SYNTHETIC_ADB_'+name).encode()
                (root/'platform-tools'/name).write_bytes(data)
                files[name] = c.sha(data)
            (root/'profiles/public-adb-3701.json').write_text(json.dumps({'files':files}))
            with patch.object(ngd, 'BASE', root):
                selected = c.persistent_adb(root/'private')
                self.assertNotEqual(selected.parent, root/'platform-tools')
                self.assertEqual(ngd.digest(selected), files['adb.exe'])
                selected.write_bytes(b'tampered cached executable')
                with self.assertRaisesRegex(RuntimeError, 'ADB checksum mismatch'):
                    c.persistent_adb(root/'private')
                self.assertEqual(selected.read_bytes(), b'tampered cached executable')

    def test_restore_reconstruction_refuses_unknown_controller_before_any_pull(self):
        with tempfile.TemporaryDirectory() as tmp:
            profile = ngd.load_profile()
            adb = Mock()
            adb.prop.return_value = profile['fingerprint']
            adb.sh.side_effect = ['0', json.dumps({'profile':profile['id'],'factory_hashes':profile['factory_files']})]
            adb.sha.return_value = '0'*64
            with patch.object(c, 'load_bundle', return_value=(Path(tmp),{})):
                with self.assertRaisesRegex(RuntimeError, 'controller/init differ'):
                    c.prepare(adb, profile, tmp, 'f'*64, restore=True)
            adb.run.assert_not_called()

    def test_bluetooth_recipe_contains_project_payload_but_excludes_oem_library(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            original = root/'source.apk'
            proprietary = b'PRIVATE-OEM-LIBRARY-CONTENT'*100
            with zipfile.ZipFile(original, 'w') as archive:
                archive.writestr('classes.dex', b'PROJECT CODE')
                archive.writestr('lib/arm64-v8a/libchrome.so', proprietary)
            exporter.recipe(original, root, 'bluetooth')
            self.assertNotIn(proprietary, (root/'bluetooth.blob').read_bytes())
            own_input = root/'libchrome.so'
            own_input.write_bytes(proprietary)
            output = root/'restored.apk'
            c.reconstruct(root/'bluetooth.recipe.json', root/'bluetooth.blob', {'libchrome.so':own_input}, output)
            self.assertEqual(output.read_bytes(), original.read_bytes())

    def test_settings_reconstruction_has_no_entry_payload_in_public_blob(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dex = bytearray(2649600)
            dex[:4] = b'dex\n'
            struct.pack_into('<I', dex, 2649532+12, 2)
            dex[2649532+16:2649532+20] = bytes.fromhex('12000f00')
            original, candidate = root/'original.apk', root/'candidate.apk'
            resources = b'PRIVATE_OEM_RESOURCE_PAYLOAD'*100
            with zipfile.ZipFile(original, 'w') as archive:
                archive.writestr('classes.dex', dex)
                archive.writestr('resources.arsc', resources)
                archive.writestr('AndroidManifest.xml', b'PRIVATE_OEM_MANIFEST'*20, compress_type=zipfile.ZIP_DEFLATED)
            original_hash = ngd.digest(original)
            with zipfile.ZipFile(original) as archive, zipfile.ZipFile(candidate, 'w') as out:
                for info in archive.infolist():
                    out.writestr(info, c.visibility_dex(archive.read(info)) if info.filename == 'classes.dex' else archive.read(info))
            exporter.recipe(candidate, root, 'settings', settings=True)
            self.assertNotIn(resources, (root/'settings.blob').read_bytes())
            c.reconstruct(root/'settings.recipe.json', root/'settings.blob', {'settings':original}, root/'recreated.apk')
            self.assertEqual(ngd.digest(root/'recreated.apk'), ngd.digest(candidate))
            self.assertEqual(ngd.digest(original), original_hash)

    def test_bad_dex_and_recipe_output_overflow_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'bytecode'):
            c.visibility_dex(b'wrong base')
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/'blob').write_bytes(b'abcdef')
            (root/'recipe').write_text(json.dumps({'schema':1, 'size':3, 'sha256':'0'*64, 'pieces':[{'literal':[0,6]}]}))
            with self.assertRaisesRegex(RuntimeError, 'exceeds'):
                c.reconstruct(root/'recipe', root/'blob', {}, root/'output')

    def test_component_paths_cannot_escape_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name in ('../escape', '/absolute', 'bad\\path'):
                with self.subTest(name=name), self.assertRaises(RuntimeError): c.safe_file(tmp, name)

    def test_wrong_oem_file_rejected_before_kit_is_published(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            original = root/'wrong'
            original.write_bytes(b'wrong base')
            with self.assertRaisesRegex(RuntimeError, 'OEM input checksum mismatch'):
                c.assemble(root, {'oem_inputs':{'settings':{'sha256':'0'*64}}}, {'settings':original}, root/'kit')
            self.assertFalse((root/'kit').exists())

    def test_changed_input_during_pull_never_builds_kit_or_writes_device(self):
        with tempfile.TemporaryDirectory() as tmp:
            profile = ngd.load_profile()
            data = b'synthetic OEM source'
            wanted = hashlib.sha256(data).hexdigest()
            index = {'oem_inputs':{'settings':{'remote':'/product/priv-app/Settings/Settings.apk','sha256':wanted}}}
            adb = Mock()
            adb.sh.return_value = '0'
            adb.prop.return_value = profile['fingerprint']
            adb.sha.side_effect = [wanted, '0'*64]
            def pull(*args, **kwargs):
                self.assertEqual(args[0], 'pull')
                Path(args[2]).write_bytes(data)
            adb.run.side_effect = pull
            with patch.object(c, 'load_bundle', return_value=(Path(tmp),index)), patch.object(ngd, 'inspect', return_value={}), \
                    patch.object(c, 'assemble') as assemble:
                with self.assertRaisesRegex(RuntimeError, 'pull checksum mismatch'):
                    c.prepare(adb, profile, tmp, 'f'*64)
                assemble.assert_not_called()
            self.assertEqual(adb.sh.call_args_list[0].args, ('id -u',))
            self.assertTrue(all(call.args[0] == 'pull' for call in adb.run.call_args_list))

    def test_bundled_components_are_complete_and_contain_no_full_apk(self):
        root, index = c.load_bundle()
        self.assertTrue(index['files'])
        self.assertFalse(any(name.endswith('.apk') for name in index['files']))
        self.assertEqual(set(index['oem_inputs']), {'settings','libbluetooth-binder.so','libchrome.so','libevent.so','libnativehelper.so'})

    def test_corrupted_bundle_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            shutil.copytree(ROOT/'installer/components', root/'components')
            with (root/'components/bluetooth.blob').open('ab') as out: out.write(b'tamper')
            with self.assertRaisesRegex(RuntimeError, 'checksum mismatch'): c.load_bundle(root/'components')


KIT = ROOT/'.local/avrcp-release-20261019'
ORIGINAL = ROOT.parent/'outputs/ETAP16_PERSISTENT/OEM_SETTINGS/Settings.original.apk'


@unittest.skipUnless((KIT/'manifest.json').is_file() and ORIGINAL.is_file(), 'OEM originals stay private; real roundtrip is a local test')
class RealLocalReconstructionTests(unittest.TestCase):
    def test_complete_read_pull_build_flow_recreates_exact_reviewed_payload(self):
        bundle, index = c.load_bundle()
        inputs = {'settings':ORIGINAL}
        for key, details in index['oem_inputs'].items():
            if key != 'settings': inputs[key] = KIT/'payload'/details['payload']
        remote = {details['remote']:inputs[key] for key,details in index['oem_inputs'].items()}
        calls = []
        class ReadOnlyAdb:
            def sh(self, command, timeout=30):
                calls.append(('sh',command))
                if command != 'id -u': raise AssertionError('Unexpected command: '+command)
                return '0'
            def prop(self, name):
                self_check = ngd.load_profile()['fingerprint']
                if name != 'ro.build.fingerprint': raise AssertionError(name)
                return self_check
            def sha(self, name): return ngd.digest(remote[name])
            def run(self, *args, **kwargs):
                calls.append(args)
                if args[0] != 'pull': raise AssertionError('Device mutation')
                shutil.copyfile(remote[args[1]], args[2])
        before = {key:ngd.digest(path) for key,path in inputs.items()}
        with tempfile.TemporaryDirectory() as tmp, patch.object(ngd, 'inspect', return_value={'profile':ngd.PROFILE,'boot':'test-only'}):
            output = c.prepare(ReadOnlyAdb(), ngd.load_profile(), tmp, 'f'*64)
            self.assertEqual(ngd.verify_release(output)['files'], ngd.verify_release(KIT)['files'])
        self.assertEqual(before, {key:ngd.digest(path) for key,path in inputs.items()})
        self.assertEqual(sum(call[0] == 'pull' for call in calls), 5)
        self.assertTrue(all(call[0] == 'pull' or call == ('sh','id -u') for call in calls))
