#!/usr/bin/env python3
"""Strict, profile-bound installer. Import/default help never contacts a device."""
from pathlib import Path
import argparse, datetime, hashlib, json, re, shlex, subprocess, sys, time

BASE = Path(sys._MEIPASS) if getattr(sys, 'frozen', False) else Path(__file__).resolve().parents[1]
DEVICE = '/data/local/ngd-native-bt'
INIT = '/system/etc/init/ngd-native-bt.rc'
PERMISSIONS = '/data/local/tmp/ngd15n-permissions.baseline'
PROFILE = 'blink6125-qti-20250217-arm64-a'

def digest(path):
    with open(path, 'rb') as f:
        h = hashlib.sha256()
        for chunk in iter(lambda: f.read(1024*1024), b''): h.update(chunk)
        return h.hexdigest()

def q(value):
    return shlex.quote(str(value))

def load_profile(path=None):
    reference = json.loads((BASE/'profiles'/(PROFILE+'.json')).read_text())
    if path is None: return reference
    profile = json.loads(Path(path).read_text(encoding='utf-8'))
    if profile != reference: validate_twin_profile(profile, reference)
    return profile


def validate_twin_profile(profile, reference=None):
    reference = reference or load_profile()
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]{2,79}', profile.get('id', '')) or profile['id'] == PROFILE:
        raise RuntimeError('STOP: invalid twin profile ID')
    fp = profile.get('fingerprint', '')
    if not fp or len(fp.encode()) > 90 or not re.fullmatch(r'[A-Za-z0-9_./:+-]+', fp):
        raise RuntimeError('STOP: invalid target fingerprint')
    for key, value in reference.items():
        if key not in ('id', 'fingerprint', 'hardware_evidence') and profile.get(key) != value:
            raise RuntimeError('STOP: new component/layout needs an engineered port: '+key)
    if profile.get('twin_family') != PROFILE:
        raise RuntimeError('STOP: unknown integration family')
    e = profile.get('hardware_evidence', {})
    if e.get('units_tested') != 0 or e.get('status') != 'TECHNICAL_CANDIDATE':
        raise RuntimeError('STOP: twin hardware verification must not be inherited')


def profile_digest(profile):
    return hashlib.sha256(json.dumps(profile, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode()).hexdigest()


def verify_release(root, profile=None):
    profile = profile or load_profile()
    root = Path(root).resolve()
    manifest = json.loads((root / 'manifest.json').read_text())
    if manifest['profile'] != profile['id']:
        raise RuntimeError('STOP: wrong release profile')
    expected = manifest['files']
    actual = {p.relative_to(root/'payload').as_posix() for p in (root/'payload').rglob('*') if p.is_file()}
    if actual != set(expected):
        raise RuntimeError('STOP: payload inventory differs')
    for rel, wanted in expected.items():
        path = (root / 'payload' / rel).resolve()
        if (root/'payload') not in path.parents or path.is_symlink() or digest(path) != wanted:
            raise RuntimeError('STOP: payload hash/path mismatch: ' + rel)
    variant = manifest.get('variant')
    if variant is not None:
        if variant != 'avrcp-legacy-20261019' or profile != load_profile():
            raise RuntimeError('STOP: unknown variant or unreviewed variant target')
        reviewed = json.loads((BASE/'profiles/payload-avrcp-20261019.json').read_text(encoding='utf-8'))
        if expected != reviewed['files']:
            raise RuntimeError('STOP: AVRCP payload differs from reviewed set')
        return manifest
    if expected['BluetoothNGD.apk'] != '23df2f6c0da3a649d03716def7c37fc0aab4944f16b0d23106c83c1b45919e90':
        raise RuntimeError('STOP: unreviewed APK; a rebuilt variant needs a new reviewed release')
    if profile['id'] == PROFILE:
        if expected['ngd16_native_bt'] != '99639abb94ba22370198080ab60a2003199c8a0a8f40145b4f55d2680b0cfd53':
            raise RuntimeError('STOP: unreviewed controller')
        reviewed = json.loads((BASE/'profiles/payload-baseline-010.json').read_text(encoding='utf-8'))
        if expected != reviewed['files']:
            raise RuntimeError('STOP: baseline payload differs from reviewed set')
    else:
        validate_twin_profile(profile)
        if manifest.get('target_profile_sha256') != profile_digest(profile):
            raise RuntimeError('STOP: release not bound to target profile')
        if profile.get('release_files') != expected:
            raise RuntimeError('STOP: target payload differs from reviewed profile')
        marker = ('NGD_TARGET_FINGERPRINT='+profile['fingerprint']).encode()+b'\0'
        for name in ('ngd16_native_bt', 'ngd16_init_installer'):
            data = (root/'payload'/name).read_bytes()
            if data[:6] != b'\x7fELF\x02\x01' or data[18:20] != b'\xb7\x00' or marker not in data:
                raise RuntimeError('STOP: wrong ARM64 target binary: '+name)

    return manifest

class Adb:
    def __init__(self, exe, serial=None):
        self.prefix = [exe] + (['-s', serial] if serial else ['-d'])
    def run(self, *args, timeout=30):
        p = subprocess.run(self.prefix + list(args), capture_output=True, timeout=timeout)
        text = p.stdout.decode('utf-8', 'replace').strip()
        if p.returncode:
            raise RuntimeError('ADB failed: ' + p.stderr.decode('utf-8', 'replace').strip() + '\n' + text)
        return text
    def sh(self, cmd, timeout=30):
        return self.run('shell', cmd, timeout=timeout)
    def prop(self, name):
        return self.sh('getprop ' + q(name))
    def sha(self, path):
        value = self.sh('sha256sum ' + q(path)).split()[0]
        if not re.fullmatch('[a-f0-9]{64}', value):
            raise RuntimeError('Invalid SHA-256 output')
        return value
    def exists(self, path):
        return self.sh('if [ -e '+q(path)+' ] || [ -L '+q(path)+' ]; then echo YES; else echo NO; fi') == 'YES'

def protections(adb, profile):
    if adb.sh('blockdev --getro ' + q(profile['root_block'])) != '1':
        raise RuntimeError('STOP: block is not RO')
    if adb.sh('cat /sys/fs/ext4/dm-3/reserved_clusters') != profile['ext4_reserved_clusters']:
        raise RuntimeError('STOP: unexpected ext4 reserve')
    lines = [x.split() for x in adb.sh('cat /proc/1/mountinfo').splitlines() if len(x.split()) > 6 and x.split()[4] == '/']
    if len(lines) != 1 or 'ro' not in lines[0][5].split(','):
        raise RuntimeError('STOP: root mount is not RO')
    fs = lines[0][lines[0].index('-')+1:]
    if fs[:2] != ['ext4', profile['root_block']]:
        raise RuntimeError('STOP: unsupported root mount/device mapping')

def inspect(adb, profile, fresh=True):
    def require(actual, wanted, label):
        if actual != wanted:
            raise RuntimeError('INCOMPATIBLE '+label+': '+repr(actual)+' expected '+repr(wanted))
    require(adb.sh('id -u'), '0', 'ADB root (run adb root yourself if supported)')
    for prop, key in [('ro.build.fingerprint','fingerprint'),('ro.build.version.sdk','sdk'),
                      ('ro.product.cpu.abi','abi'),('ro.boot.slot_suffix','slot'),('ro.boot.verifiedbootstate','verified_boot')]:
        require(adb.prop(prop), profile[key], prop)
    require(adb.sh('getenforce'), profile['selinux'], 'SELinux')
    require(adb.prop('sys.boot_completed'), '1', 'boot_completed')
    if profile.get('twin_family'):
        require(adb.prop('ro.board.platform'), 'trinket', 'hardware platform')
        values = {adb.prop(k).lower() for k in ('qcom.bluetooth.soc', 'vendor.qcom.bluetooth.soc')} - {''}
        require(values, {'cherokee'}, 'controller identity')
    protections(adb, profile)
    for path, sha in profile['factory_files'].items():
        require(adb.sha(path), sha, path)
    package = adb.sh('dumpsys package ' + q(profile['package']))
    if not re.search(r'\buserId=1002\b', package) or 'android.uid.bluetooth' not in package:
        raise RuntimeError('STOP: original shared UID identity differs')
    if adb.sh('pm list packages --uid 1002') != 'package:org.codeaurora.bluetooth':
        raise RuntimeError('STOP: additional shared-UID1002 packages; no replacement attempted')
    require(adb.sh('pm path '+profile['package']), 'package:/system/app/BluetoothExt/BluetoothExt.apk', 'package path')
    users = adb.sh('pm list users')
    if users.count('UserInfo{') != 1 or 'UserInfo{0:' not in users:
        raise RuntimeError('STOP: only single-user factory setup supported')
    require(adb.prop('init.svc.blink'), 'running', 'OEM BLINK')
    require(adb.sh('settings get global bluetooth_on'), '0', 'factory native BT setting')
    mcu = adb.sh('pidof com.carocean.mcuserver')
    if not mcu.isdigit():
        raise RuntimeError('STOP: MCU is absent or ambiguous')
    for name, value in profile['baseline_audio'].items():
        require(adb.prop(name), value, name)
    tiles = adb.sh('settings --user 0 get secure sysui_qs_tiles')
    if not tiles or tiles == 'null':
        raise RuntimeError('STOP: unresolved OEM default tile list. Save the current Quick Settings layout in its editor, then check again; do not guess a replacement list.')
    if fresh:
        for path in (DEVICE, INIT, PERMISSIONS):
            if adb.exists(path):
                raise RuntimeError('STOP: existing installation/baseline; no overwrite: '+path)
    return {'profile':profile['id'],'boot':adb.sh('cat /proc/sys/kernel/random/boot_id'),
            'mcu':mcu,'package':package,'factory_hashes':profile['factory_files'],
            'audio':{k:adb.prop(k) for k in profile['baseline_audio']},
            'bluetooth_on':adb.sh('settings get global bluetooth_on'),
            'ble_scan':adb.sh('settings get global ble_scan_always_enabled'),
            'tiles':tiles}

def put_verified(adb, local, remote, mode):
    adb.run('push', str(local), remote, timeout=120)
    adb.sh('chown 0:0 '+q(remote)+' && chmod '+mode+' '+q(remote))
    if adb.sha(remote) != digest(local):
        raise RuntimeError('STOP: device readback differs '+remote)

def record(path, data):
    path.write_bytes((json.dumps(data, indent=2, ensure_ascii=False)+'\n').encode('utf-8'))

def install(adb, profile, release, backup_root, args):
    if not (args.accept_system_write and args.accept_whole_blink_pause and args.recovery_confirmed):
        raise RuntimeError('No changes: installation needs --accept-system-write --accept-whole-blink-pause --recovery-confirmed')
    manifest = verify_release(release, profile)
    if profile['id'] != PROFILE and not getattr(args, 'accept_unverified_twin', False):
        raise RuntimeError('No changes: candidate hardware requires --accept-unverified-twin')
    baseline = inspect(adb, profile)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    backup = Path(backup_root).resolve()/stamp
    backup.mkdir(parents=True, exist_ok=False)
    record(backup/'baseline.json', baseline)
    # Pull every pinned original before the first device write. Originals never enter Git.
    for remote, expected in profile['factory_files'].items():
        local = backup/'originals'/remote.lstrip('/')
        local.parent.mkdir(parents=True, exist_ok=True)
        adb.run('pull', remote, str(local), timeout=120)
        if digest(local) != expected:
            raise RuntimeError('STOP: original backup checksum mismatch')
    adb.sh('mkdir -m 0700 '+DEVICE)
    adb.sh('mkdir -m 0700 '+DEVICE+'/view')
    adb.sh('mkdir -m 0755 '+DEVICE+'/view/candidate')
    adb.sh('mkdir -m 0755 '+DEVICE+'/view/candidate/lib')
    adb.sh('mkdir -m 0755 '+DEVICE+'/view/candidate/lib/arm64')
    payload = Path(release)/'payload'
    destinations = {'BluetoothNGD.apk':('view/candidate/BluetoothExt.apk','0644'),
                    'ngd16_native_bt':('ngd16_native_bt','0700'),
                    'ngd_blink_hold12':('ngd_blink_hold12','0700'),
                    'ngd16_init_installer':('ngd16_init_installer','0700'),
                    'Settings.bluetooth-visible.apk':('Settings.bluetooth-visible.apk','0600'),
                    'permissions.jar':('permissions.jar','0600')}
    for name in manifest['files']:
        if name.startswith('lib/arm64/'):
            destinations[name] = ('view/candidate/'+name,'0644')
    for src, (dst, mode) in destinations.items():
        put_verified(adb, payload/src, DEVICE+'/'+dst, mode)
    put_verified(adb, backup/'baseline.json', DEVICE+'/distribution-baseline.json', '0600')
    # Creates a NEW per-device config/permissions snapshot, never imports another radio's data.
    adb.sh(DEVICE+'/ngd16_native_bt --capture-oem '+DEVICE+'/factory.receipt', timeout=90)
    for remote in (DEVICE+'/oem.snapshot', DEVICE+'/factory.receipt', PERMISSIONS):
        dest = backup/Path(remote).name
        adb.run('pull', remote, str(dest), timeout=120)
        if digest(dest) != adb.sha(remote):
            raise RuntimeError('STOP: config snapshot readback mismatch')
    adb.sh(DEVICE+'/ngd16_native_bt --verify-payload')
    if adb.sh('pidof com.carocean.mcuserver') != baseline['mcu']:
        raise RuntimeError('STOP: MCU changed; no system activation')
    helper = DEVICE+'/ngd16_init_installer'
    init_sha = manifest['files']['ngd-native-bt.rc']
    installed = False
    try:
        # Exercise exact new-file install/remove on THIS radio, before autostart/audio changes.
        adb.sh(helper+' --install-approved', timeout=40); installed = True
        if adb.sha(INIT) != init_sha:
            raise RuntimeError('STOP: init readback mismatch')
        protections(adb, profile)
        adb.sh(helper+' --remove-approved', timeout=40); installed = False
        if adb.exists(INIT):
            raise RuntimeError('STOP: init removal exercise failed')
        protections(adb, profile)
        if adb.prop('init.svc.blink') != 'running' or adb.sh('pidof com.carocean.mcuserver') != baseline['mcu']:
            raise RuntimeError('STOP: OEM services changed during write exercise')
        record(backup/'init-write-remove-pass.json', {'pass':True,'init_sha':init_sha,'ro_restored':True})
        for name, value in profile['install_audio'].items():
            adb.sh('setprop '+q(name)+' '+q(value))
            if adb.prop(name) != value: raise RuntimeError('Audio property readback failed')
        # Init is parsed only at next boot. Marker is created exclusively, root0600.
        adb.sh('(set -C; umask 077; : > '+DEVICE+'/enabled)')
        adb.sh('chmod 0600 '+DEVICE+'/enabled')
        adb.sh(helper+' --install-approved', timeout=40); installed = True
        if adb.sha(INIT) != init_sha: raise RuntimeError('Final init readback mismatch')
        protections(adb, profile)
        tiles = baseline['tiles']
        if tiles != 'null':
            parts = tiles.split(',')
            if 'bt' not in parts:
                parts.insert(parts.index('wifi')+1 if 'wifi' in parts else 0, 'bt')
                after = ','.join(parts)
                adb.sh('settings --user 0 put secure sysui_qs_tiles '+q(after))
                if adb.sh('settings --user 0 get secure sysui_qs_tiles') != after:
                    raise RuntimeError('Tile readback failed')
        record(backup/'activation.json', {'installed':True,'reboot_requested':False,'init_sha':init_sha,
                                         'package_manager_install_or_uninstall':False,'manifest':manifest})
        print('INSTALLED. No reboot performed. Backup: '+str(backup))
        print('NEXT: adb reboot, only when prepared for the whole BLINK/Android interruption.')
    except Exception:
        # Never remove an unknown file, erase backups, or try to force a reboot.
        if adb.exists(DEVICE+'/enabled'):
            adb.sh('rm '+DEVICE+'/enabled')
        if adb.exists(INIT) and adb.sha(INIT) == init_sha:
            protections(adb, profile)
            adb.sh(helper+' --remove-approved', timeout=40)
        for name, value in baseline['audio'].items(): adb.sh('setprop '+q(name)+' '+q(value))
        set_setting(adb, '--user 0 secure', 'sysui_qs_tiles', baseline['tiles'])
        record(backup/'activation-failed.json', {'activation':False,'backups_preserved':True})
        raise

def set_setting(adb, namespace, name, value):
    parts = namespace.split(); ns = parts[-1]
    prefix = ' '.join(parts[:-1])
    command = 'settings '+(prefix+' ' if prefix else '')
    if value == 'null': adb.sh(command+'delete '+ns+' '+q(name))
    else: adb.sh(command+'put '+ns+' '+q(name)+' '+q(value))

def status(adb):
    print(adb.sh('getprop init.svc.ngd-native-bt; getprop init.svc.blink; pidof com.carocean.mcuserver'))
    print(adb.sh('tail -25 '+DEVICE+'/boot-prepare.log; tail -15 '+DEVICE+'/runtime.log'))
    text = adb.sh('dumpsys bluetooth_manager')
    print(text.split('--- BEGIN:BTSNOOP_LOG_SUMMARY',1)[0])

def remove(adb, profile, release, args):
    if not args.accept_oem_return:
        raise RuntimeError('No changes: --accept-oem-return is required')
    manifest = verify_release(release, profile)
    if adb.prop('ro.build.fingerprint') != profile['fingerprint'] or adb.sh('id -u') != '0':
        raise RuntimeError('STOP: wrong device or no root')
    if adb.sha(INIT) != manifest['files']['ngd-native-bt.rc']:
        raise RuntimeError('STOP: init changed; no removal')
    if adb.sha(DEVICE+'/ngd16_native_bt') != manifest['files']['ngd16_native_bt']:
        raise RuntimeError('STOP: controller changed; inspect manually')
    baseline = json.loads(adb.sh('cat '+DEVICE+'/distribution-baseline.json'))
    if baseline['profile'] != profile['id'] or baseline['factory_hashes'] != profile['factory_files']:
        raise RuntimeError('STOP: baseline differs')
    if adb.prop('init.svc.ngd-native-bt') == 'running':
        adb.sh(DEVICE+'/ngd16_native_bt --restore-now')
    for _ in range(40):
        if adb.prop('init.svc.ngd-native-bt') in ('stopped','') and not adb.exists(DEVICE+'/session.journal'):
            if adb.sh('if pidof com.android.bluetooth.ngd >/dev/null; then echo LIVE; else echo GONE; fi') == 'GONE': break
        time.sleep(1)
    else: raise RuntimeError('STOP: native closure not confirmed; no init removal/reboot')
    protections(adb, profile)
    # Only our marker/init are removed. The live RAM bindings stay until an explicit reboot.
    if adb.exists(DEVICE+'/enabled'): adb.sh('rm '+DEVICE+'/enabled')
    adb.sh(DEVICE+'/ngd16_init_installer --remove-approved', timeout=40)
    if adb.exists(INIT): raise RuntimeError('STOP: init still exists')
    protections(adb, profile)
    for name, value in baseline['audio'].items(): adb.sh('setprop '+q(name)+' '+q(value))
    set_setting(adb, 'global', 'bluetooth_on', baseline['bluetooth_on'])
    set_setting(adb, 'global', 'ble_scan_always_enabled', baseline['ble_scan'])
    set_setting(adb, '--user 0 secure', 'sysui_qs_tiles', baseline['tiles'])
    # Restore saved package user enabled state, never manipulate shared UID/database ownership.
    m = re.search(r'User 0:.*?\benabled=(\d+)', baseline['package'])
    if not m or m[1] not in ('0','3'): raise RuntimeError('Unexpected saved package state')
    adb.sh(('pm default-state' if m[1]=='0' else 'pm disable-user')+' --user 0 org.codeaurora.bluetooth')
    adb.sh('env CLASSPATH='+DEVICE+'/permissions.jar app_process /system/bin Ngd15NativePermissions restore')
    print('OEM RETURN PREPARED; no reboot performed. Backups and payload preserved.')
    print('NEXT: approved adb reboot; RAM bindings disappear and OEM files return. Verify MCU/BLINK afterward.')

def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--adb', default='adb'); p.add_argument('--serial')
    p.add_argument('--profile', type=Path, help='Explicit per-target twin profile; no force bypass')
    p.add_argument('--release', type=Path, default=BASE/'release')
    p.add_argument('--backups', type=Path, default=BASE/'.local/backups')
    subs = p.add_subparsers(dest='action', required=True)
    for name in ['check','verify-release','plan','status']: subs.add_parser(name)
    i = subs.add_parser('install')
    for opt in ['accept-system-write','accept-whole-blink-pause','recovery-confirmed']: i.add_argument('--'+opt, action='store_true')
    i.add_argument('--accept-unverified-twin', action='store_true')
    u = subs.add_parser('remove'); u.add_argument('--accept-oem-return', action='store_true')
    args = p.parse_args(argv); profile = load_profile(args.profile)
    if args.action == 'verify-release': print(json.dumps(verify_release(args.release, profile),indent=2)); return
    if args.action == 'plan':
        print('TARGET PROFILE: '+profile['id']+' / '+profile['fingerprint'])
        print('Per-device backups; create /data payload; test new init install/remove; add only '+INIT)
        print('Whole BLINK is held/stopped at next boot; MCU application is separate but all functions are not verified.')
        print('Audio properties false/true; RAM binds replace BluetoothExt and Settings at boot; no pm install/uninstall.')
        print('NO automatic reboot, flash, SELinux change, UID repair, or unsupported-profile bypass.')
        print('Shared package identity1002 and root/ext4/dm-3/_a/original hashes must match. Recovery is owner-confirmed, not detected.')
        return
    adb = Adb(args.adb, args.serial)
    if args.action == 'check': print(json.dumps(inspect(adb,profile),indent=2))
    elif args.action == 'install': install(adb,profile,args.release,args.backups,args)
    elif args.action == 'status': status(adb)
    else: remove(adb,profile,args.release,args)

if __name__ == '__main__':
    try: main()
    except (RuntimeError, OSError, subprocess.TimeoutExpired) as exc:
        raise SystemExit('STOP: '+str(exc))
