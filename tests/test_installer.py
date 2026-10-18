import contextlib, importlib.util, io, json, tempfile, unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ngd_patch', ROOT/'tools/ngd_patch.py')
ngd = importlib.util.module_from_spec(spec); spec.loader.exec_module(ngd)

class FakeAdb:
    def __init__(self):
        self.profile=ngd.load_profile(); self.calls=[]; self.files=set(); self.uid='0'
        self.props={k:self.profile[v] for k,v in [('ro.build.fingerprint','fingerprint'),('ro.build.version.sdk','sdk'),('ro.product.cpu.abi','abi'),('ro.boot.slot_suffix','slot'),('ro.boot.verifiedbootstate','verified_boot')]}
        self.props.update(self.profile['baseline_audio']); self.props.update({'sys.boot_completed':'1','init.svc.blink':'running'})
    def prop(self,k): return self.props.get(k,'')
    def exists(self,p): return p in self.files
    def sha(self,p): return self.profile['factory_files'][p]
    def sh(self,c,timeout=30):
        self.calls.append(c)
        known={'id -u':self.uid,'getenforce':'Permissive','blockdev --getro /dev/block/dm-3':'1',
               'cat /sys/fs/ext4/dm-3/reserved_clusters':'4096',
               'cat /proc/1/mountinfo':'24 1 253:3 / / ro,relatime - ext4 /dev/block/dm-3 ro',
               'dumpsys package org.codeaurora.bluetooth':'userId=1002 sharedUser=android.uid.bluetooth\nUser 0: ceDataInode=123 enabled=0',
               'pm path org.codeaurora.bluetooth':'package:/system/app/BluetoothExt/BluetoothExt.apk',
               'pm list packages --uid 1002':'package:org.codeaurora.bluetooth',
               'pm list users':'Users: UserInfo{0:Owner:13}',
               'settings get global bluetooth_on':'0','pidof com.carocean.mcuserver':'1234',
               'cat /proc/sys/kernel/random/boot_id':'00000000-1111-2222-3333-444444444444',
               'settings get global ble_scan_always_enabled':'1','settings --user 0 get secure sysui_qs_tiles':'wifi,cell'}
        if c not in known: raise AssertionError('Unexpected or mutating command: '+c)
        return known[c]

class GuardTests(unittest.TestCase):
    def test_exact_factory_profile_passes_without_writes(self):
        a=FakeAdb(); self.assertEqual(ngd.inspect(a,a.profile)['mcu'],'1234')
    def test_same_soc_different_firmware_refused(self):
        a=FakeAdb(); a.props['ro.build.fingerprint']='qti/another:10/test'
        with self.assertRaisesRegex(RuntimeError,'INCOMPATIBLE'): ngd.inspect(a,a.profile)
        self.assertFalse(any('mkdir' in c for c in a.calls))
    def test_missing_root_refused(self):
        a=FakeAdb(); a.uid='2000'
        with self.assertRaisesRegex(RuntimeError,'ADB root'): ngd.inspect(a,a.profile)
    def test_existing_install_never_overwritten(self):
        a=FakeAdb(); a.files.add(ngd.DEVICE)
        with self.assertRaisesRegex(RuntimeError,'no overwrite'): ngd.inspect(a,a.profile)
    def test_existing_permission_baseline_never_overwritten(self):
        a=FakeAdb(); a.files.add(ngd.PERMISSIONS)
        with self.assertRaisesRegex(RuntimeError,'no overwrite'): ngd.inspect(a,a.profile)
    def test_wrong_slot_refused(self):
        a=FakeAdb(); a.props['ro.boot.slot_suffix']='_b'
        with self.assertRaisesRegex(RuntimeError,'slot'): ngd.inspect(a,a.profile)
    def test_original_hash_mismatch_refused(self):
        a=FakeAdb(); a.sha=lambda path:'0'*64
        with self.assertRaisesRegex(RuntimeError,'INCOMPATIBLE'): ngd.inspect(a,a.profile)
    def test_root_rw_is_not_assumed_safe(self):
        a=FakeAdb(); orig=a.sh
        a.sh=lambda c,timeout=30:'0' if c.startswith('blockdev --getro') else orig(c)
        with self.assertRaisesRegex(RuntimeError,'not RO'): ngd.inspect(a,a.profile)
    def test_additional_shared_uid_package_refused(self):
        a=FakeAdb(); orig=a.sh
        a.sh=lambda c,timeout=30:'package:org.codeaurora.bluetooth\npackage:other.bluetooth' if c=='pm list packages --uid 1002' else orig(c)
        with self.assertRaisesRegex(RuntimeError,'additional shared-UID'): ngd.inspect(a,a.profile)
    def test_unknown_default_tile_list_not_replaced(self):
        a=FakeAdb(); orig=a.sh
        a.sh=lambda c,timeout=30:'null' if c=='settings --user 0 get secure sysui_qs_tiles' else orig(c)
        with self.assertRaisesRegex(RuntimeError,'default tile list'): ngd.inspect(a,a.profile)
    def test_setting_restore_uses_correct_android_command_order(self):
        class Recorder:
            def sh(self,c): self.c=c
        a=Recorder(); ngd.set_setting(a,'--user 0 secure','sysui_qs_tiles','wifi,bt')
        self.assertEqual(a.c,'settings --user 0 put secure sysui_qs_tiles wifi,bt')
        ngd.set_setting(a,'global','bluetooth_on','null')
        self.assertEqual(a.c,'settings delete global bluetooth_on')
    def test_install_without_explicit_acknowledgements_has_no_device_calls(self):
        from types import SimpleNamespace
        a=FakeAdb()
        with self.assertRaisesRegex(RuntimeError,'No changes'):
            ngd.install(a,a.profile,ROOT/'release',ROOT/'.local',SimpleNamespace(accept_system_write=False,accept_whole_blink_pause=False,recovery_confirmed=False))
        self.assertEqual(a.calls,[])
    def test_plan_has_no_subprocess_or_adb_calls(self):
        with patch.object(ngd.subprocess,'run',side_effect=AssertionError('device call')), contextlib.redirect_stdout(io.StringIO()):
            ngd.main(['plan'])
    def test_payload_tampering_refused(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d); (p/'payload').mkdir(); (p/'payload/fake').write_bytes(b'bad')
            (p/'manifest.json').write_text(json.dumps({'profile':ngd.PROFILE,'files':{'fake':'0'*64}}))
            with self.assertRaisesRegex(RuntimeError,'hash/path mismatch'): ngd.verify_release(p)

if __name__=='__main__': unittest.main()
