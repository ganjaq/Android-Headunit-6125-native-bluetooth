import copy, importlib.util, json, sys, tempfile, unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import ngd_patch as ngd
import ngd_twins as twins
spec=importlib.util.spec_from_file_location('baseline_tests',ROOT/'tests/test_installer.py')
base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)

class TwinTests(unittest.TestCase):
    def fake(self):
        a=base.FakeAdb();a.props['ro.board.platform']='trinket';a.props['qcom.bluetooth.soc']='cherokee'
        a.props['ro.build.fingerprint']='vendor/twin/trinket:10/QKQ1.200816.002/build.20261018:userdebug/test-keys'
        return a
    def candidate(self): return twins.assess(self.fake())
    def test_other_fingerprint_is_candidate_without_device_writes(self):
        a=self.fake();r=twins.assess(a);self.assertEqual(r['status'],'TECHNICAL_CANDIDATE')
        self.assertFalse(r['hardware_tested']);self.assertTrue(a.calls)
    def test_component_difference_needs_port_not_force(self):
        a=self.fake();original=a.sha;a.sha=lambda p:'0'*64 if p.endswith('libbluetooth_qti.so') else original(p)
        r=twins.assess(a);self.assertEqual(r['status'],'NEEDS_EVIDENCE');self.assertEqual(len(r['component_differences']),1)
    def test_all_component_differences_collected(self):
        a=self.fake();a.sha=lambda p:'0'*64;r=twins.assess(a)
        self.assertEqual(len(r['component_differences']),len(ngd.load_profile()['factory_files']))
    def test_same_name_without_platform_evidence_not_accepted(self):
        a=self.fake();a.props['ro.board.platform']='other';self.assertEqual(twins.assess(a)['status'],'NEEDS_EVIDENCE')
    def test_missing_controller_identity_needs_evidence(self):
        a=self.fake();a.props['qcom.bluetooth.soc']='';self.assertEqual(twins.assess(a)['status'],'NEEDS_EVIDENCE')
    def test_existing_modified_unit_not_candidate(self):
        a=self.fake();a.files.add(ngd.DEVICE);self.assertEqual(twins.assess(a)['status'],'NEEDS_EVIDENCE')
    def test_prepare_changes_both_native_targets_and_drops_inherited_success(self):
        with tempfile.TemporaryDirectory() as d:
            out=Path(d)/'target';p=twins.prepare(self.candidate(),'trinket-test-twin',out)
            self.assertEqual(p['hardware_evidence']['units_tested'],0)
            for rel in ['session/ngd_native_session_main.cpp','installer/init_installer.cpp']:
                s=(out/'src'/rel).read_text(encoding='utf-8');self.assertIn(p['fingerprint'],s)
                self.assertNotIn(ngd.load_profile()['fingerprint'],s)
            self.assertEqual(p['factory_files'],ngd.load_profile()['factory_files'])
            for rel in ('session/ngd12_artifact_digests.h', 'android-app/jni/ngd12_artifact_digests.h'):
                pins = (out/'src'/rel).read_text(encoding='utf-8')
                self.assertIn('23df2f6c0da3a649d03716def7c37fc0aab4944f16b0d23106c83c1b45919e90', pins)
                self.assertNotIn('NGD_AVRCP_BRIDGE_SHA', pins)
            with self.assertRaisesRegex(RuntimeError,'no overwrite'):twins.prepare(self.candidate(),'trinket-test-twin',out)
    def test_candidate_cannot_weaken_partition_or_runtime_checks(self):
        with tempfile.TemporaryDirectory() as d:
            p=twins.prepare(self.candidate(),'trinket-test-twin',Path(d)/'target')
            for key,value in [('slot','_b'),('root_block','/dev/block/dm-5'),('factory_files',{})]:
                q=copy.deepcopy(p);q[key]=value
                with self.assertRaisesRegex(RuntimeError,'engineered port'):ngd.validate_twin_profile(q)
    def test_failed_assessment_not_prepared(self):
        r=self.candidate();r['status']='NEEDS_EVIDENCE'
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(RuntimeError,'complete read-only'):twins.prepare(r,'test-twin',Path(d)/'target')
    def test_inconsistent_report_refused(self):
        r=self.candidate();r['factory_hashes']={}
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(RuntimeError,'inconsistent'):twins.prepare(r,'test-twin',Path(d)/'target')
    def test_controller_identity_rechecked_on_target_inspect(self):
        with tempfile.TemporaryDirectory() as d:
            p=twins.prepare(self.candidate(),'test-twin',Path(d)/'target');a=self.fake()
            a.props['qcom.bluetooth.soc']='other'
            with self.assertRaisesRegex(RuntimeError,'controller identity'): ngd.inspect(a,p)
    def test_twin_release_profile_tampering_refused(self):
        # A profile changeset must invalidate the sealed release binding.
        with tempfile.TemporaryDirectory() as d:
            p=twins.prepare(self.candidate(),'test-twin',Path(d)/'target')
            p['release_files']={};p['note']='changed after sealing'
            root=Path(d)/'kit';(root/'payload').mkdir(parents=True)
            data=b'unchanged';(root/'payload/BluetoothNGD.apk').write_bytes(data)
            manifest={'profile':p['id'],'target_profile_sha256':'0'*64,'files':{'BluetoothNGD.apk':ngd.digest(root/'payload/BluetoothNGD.apk')}}
            (root/'manifest.json').write_text(json.dumps(manifest))
            with patch.object(ngd,'digest',return_value='23df2f6c0da3a649d03716def7c37fc0aab4944f16b0d23106c83c1b45919e90'):
                manifest['files']['BluetoothNGD.apk']='23df2f6c0da3a649d03716def7c37fc0aab4944f16b0d23106c83c1b45919e90'
                (root/'manifest.json').write_text(json.dumps(manifest))
                with self.assertRaisesRegex(RuntimeError,'not bound to target'): ngd.verify_release(root,p)
    def test_unbuilt_old_binaries_cannot_be_sealed(self):
        with tempfile.TemporaryDirectory() as d:
            target=Path(d)/'target';twins.prepare(self.candidate(),'test-twin',target)
            # Synthetic reference-target ELF: this test checks rejection of an
            # old target, not the separately distributed OEM release payload.
            old_binary=Path(d)/'reference-target.elf'
            data=bytearray(64);data[:6]=b'\x7fELF\x02\x01';data[18:20]=b'\xb7\x00'
            data.extend(('NGD_TARGET_FINGERPRINT='+ngd.load_profile()['fingerprint']).encode()+b'\0')
            old_binary.write_bytes(data)
            with patch.object(twins,'verify_release',return_value={'version':'synthetic-test-only'}) as verified:
                with self.assertRaisesRegex(RuntimeError,'target-specific ARM64'):
                    twins.seal(target,old_binary,old_binary,Path(d)/'synthetic-release',Path(d)/'kit',True)
                verified.assert_called_once()
    def test_no_acknowledgement_no_seal(self):
        with self.assertRaisesRegex(RuntimeError,'ack-reviewed-build'):twins.seal('x','x','x','x','x')
    def test_reference_release_policy_accepts_pinned_hashes(self):
        # No OEM binaries in public CI. Digest results are explicit test doubles;
        # this verifies the reference-profile policy, not real artifact integrity.
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'payload').mkdir()
            expected={'BluetoothNGD.apk':'23df2f6c0da3a649d03716def7c37fc0aab4944f16b0d23106c83c1b45919e90',
                      'ngd16_native_bt':'99639abb94ba22370198080ab60a2003199c8a0a8f40145b4f55d2680b0cfd53'}
            for name in expected:(root/'payload'/name).write_bytes(b'SYNTHETIC TEST ONLY')
            (root/'manifest.json').write_text(json.dumps({'profile':ngd.PROFILE,'files':expected}))
            with patch.object(ngd,'digest',side_effect=lambda path:expected[Path(path).name]):
                self.assertEqual(ngd.verify_release(root)['profile'],ngd.PROFILE)
                expected['ngd16_native_bt']='0'*64
                (root/'manifest.json').write_text(json.dumps({'profile':ngd.PROFILE,'files':expected}))
                with self.assertRaisesRegex(RuntimeError,'unreviewed controller'):ngd.verify_release(root)
    @unittest.skipUnless((ROOT/'release/manifest.json').is_file(),
                         'Local release/OEM payload not distributed in the source repository')
    def test_real_default_release_still_verified(self):
        self.assertEqual(ngd.verify_release(ROOT/'release')['profile'],ngd.PROFILE)
    def test_twin_cannot_install_with_only_old_approvals(self):
        from types import SimpleNamespace
        with tempfile.TemporaryDirectory() as d:
            p=twins.prepare(self.candidate(),'test-twin',Path(d)/'target');a=self.fake()
            args=SimpleNamespace(accept_system_write=True,accept_whole_blink_pause=True,recovery_confirmed=True)
            with patch.object(ngd,'verify_release',return_value={}):
                with self.assertRaisesRegex(RuntimeError,'accept-unverified-twin'):ngd.install(a,p,'x',d,args)
            self.assertEqual(a.calls,[])

if __name__=='__main__':unittest.main()
