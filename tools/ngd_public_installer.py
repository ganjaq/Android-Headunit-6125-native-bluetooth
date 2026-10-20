#!/usr/bin/env python3
"""Public desktop frontend for the existing guarded ADB installer. No server."""
from pathlib import Path
from types import SimpleNamespace
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

import ngd_patch as ngd

VERSION = '0.3.0-rc1'


def run_adb(exe, *args, timeout=30):
    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
    p = subprocess.run([str(exe), *map(str, args)], capture_output=True, timeout=timeout, **options)
    if p.returncode:
        raise RuntimeError(p.stderr.decode('utf-8', 'replace').strip() or 'ADB failed')
    return p.stdout.decode('utf-8', 'replace').strip()


def select_transport(output):
    rows = [line.split() for line in output.splitlines()[1:] if line.strip()]
    if len(rows) != 1:
        raise RuntimeError('Connect exactly one device / Podłącz dokładnie jedno urządzenie')
    row = rows[0]
    if len(row) < 2 or row[1] != 'device':
        raise RuntimeError('Unlock the radio and authorize USB debugging / Odblokuj radio i zatwierdź debugowanie USB')
    values = [x.split(':', 1)[1] for x in row[2:] if x.startswith('transport_id:')]
    if len(values) != 1 or not values[0].isdigit():
        raise RuntimeError('Use current official Android platform-tools / Użyj aktualnych oficjalnych platform-tools')
    return values[0]


class TransportAdb(ngd.Adb):
    def __init__(self, exe, transport):
        self.exe, self.transport = str(exe), transport

    def run(self, *args, timeout=30):
        # Do not silently retarget a newly connected device mid-operation.
        if select_transport(run_adb(self.exe, 'devices', '-l')) != self.transport:
            raise RuntimeError('ADB connection changed; check again / Zmieniło się połączenie ADB; sprawdź ponownie')
        return run_adb(self.exe, '-t', self.transport, *args, timeout=timeout)


def connect(exe, request_root=False):
    adb = TransportAdb(exe, select_transport(run_adb(exe, 'devices', '-l')))
    fingerprint = adb.prop('ro.build.fingerprint')
    if adb.prop('ro.board.platform') != 'trinket' or adb.prop('ro.build.version.sdk') != '29':
        raise RuntimeError('This installer requires trinket Android 10/API29 / Wymagany trinket Android 10/API29')
    identity = device_identity(adb)
    if request_root and adb.sh('id -u') != '0':
        run_adb(exe, '-t', adb.transport, 'root')
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            time.sleep(1)
            try:
                adb = TransportAdb(exe, select_transport(run_adb(exe, 'devices', '-l')))
                if adb.sh('id -u') == '0':
                    break
            except RuntimeError:
                continue
        else:
            raise RuntimeError('ADB root unavailable / Root przez ADB niedostępny')
        if adb.prop('ro.build.fingerprint') != fingerprint or device_identity(adb) != identity:
            raise RuntimeError('Device identity changed after ADB reconnect / Zmieniła się tożsamość urządzenia')
    return adb


def device_identity(adb):
    # No Bluetooth addresses, pairing data, accounts or raw identifiers in reports.
    values = [adb.prop('ro.serialno'), adb.prop('ro.boot.serialno'),
              adb.sh('settings --user 0 get secure android_id')]
    usable = [v for v in values if v and v not in ('null', 'unknown', '?', '0')]
    if not usable:
        raise RuntimeError('Stable device identity unavailable / Brak identyfikatora urządzenia')
    return hashlib.sha256(json.dumps(values, separators=(',', ':')).encode()).hexdigest()


def state_hash(baseline):
    return hashlib.sha256(json.dumps({k: v for k, v in baseline.items() if k != 'package'},
                                    sort_keys=True, separators=(',', ':')).encode()).hexdigest()


class Installer:
    def __init__(self, exe, release, data_dir, profile=None, root=True):
        self.exe, self.release, self.data_dir = str(exe), Path(release) if release else None, Path(data_dir)
        self.profile = ngd.load_profile(profile)
        self.root = root
        self.checked = None

    def _remember_kit(self, identity):
        directory = self.data_dir/'receipts'
        directory.mkdir(parents=True, exist_ok=True)
        ngd.record(directory/(identity+'.json'), {'identity':identity, 'profile_sha256':ngd.profile_digest(self.profile),
                   'release':str(self.release.resolve()), 'manifest_sha256':ngd.digest(self.release/'manifest.json')})

    def _ensure_release(self, adb, restore=False):
        if self.release is not None:
            ngd.verify_release(self.release, self.profile)
            return
        identity = device_identity(adb)
        receipt_path = self.data_dir/'receipts'/(identity+'.json')
        if receipt_path.is_file():
            receipt = json.loads(receipt_path.read_text(encoding='utf-8'))
            if receipt['identity'] != identity or receipt['profile_sha256'] != ngd.profile_digest(self.profile):
                raise RuntimeError('Local kit receipt identity/profile mismatch')
            release = Path(receipt['release'])
            if release.is_dir():
                ngd.verify_release(release, self.profile)
                if ngd.digest(release/'manifest.json') != receipt['manifest_sha256']:
                    raise RuntimeError('Saved manifest changed')
                self.release = release
                return
        import ngd_components
        self.release = ngd_components.prepare(adb, self.profile, self.data_dir, identity, restore=restore)
        self._remember_kit(identity)

    def check(self):
        self.checked = None
        adb = connect(self.exe, self.root)
        facts = {'schema': 1, 'installer': VERSION, 'fingerprint': adb.prop('ro.build.fingerprint'),
                 'device_identity_sha256': device_identity(adb), 'profile': self.profile['id'],
                 'adb_uid': adb.sh('id -u'), 'native_service': adb.prop('init.svc.ngd-native-bt')}
        if adb.exists(ngd.DEVICE) or adb.exists(ngd.INIT):
            facts.update(status='EXISTING_INSTALLATION',
                         message='Existing modification: individual update review required; no overwrite. / Istniejąca modyfikacja: wymagana ocena aktualizacji; bez nadpisywania.')
            return facts
        try:
            if self.release is None:
                ngd.inspect(adb, self.profile)
            self._ensure_release(adb)
            manifest = ngd.verify_release(self.release, self.profile)
            baseline = ngd.inspect(adb, self.profile)
        except (RuntimeError, OSError, ValueError, KeyError) as exc:
            facts.update(status='NOT_READY', message=str(exc))
            return facts
        self.checked = {'identity': facts['device_identity_sha256'], 'state': state_hash(baseline),
                        'manifest': ngd.digest(self.release/'manifest.json')}
        facts.update(status='READY', version=manifest['version'], message='Checks passed / Kontrole zakończone poprawnie')
        return facts

    def install(self, accepted=False):
        if not accepted or not self.checked:
            raise RuntimeError('Check the radio and accept the installation first / Najpierw sprawdź radio i zatwierdź instalację')
        checked, self.checked = self.checked, None
        adb = connect(self.exe, self.root)
        ngd.verify_release(self.release, self.profile)
        baseline = ngd.inspect(adb, self.profile)
        if (device_identity(adb) != checked['identity'] or state_hash(baseline) != checked['state'] or
                ngd.digest(self.release/'manifest.json') != checked['manifest']):
            raise RuntimeError('Radio state or kit changed; check again / Stan radia lub paczka zmieniły się; sprawdź ponownie')
        self.data_dir.mkdir(parents=True, exist_ok=True)
        self._remember_kit(checked['identity'])
        args = SimpleNamespace(accept_system_write=True, accept_whole_blink_pause=True,
                               recovery_confirmed=True, accept_unverified_twin=self.profile['id'] != ngd.PROFILE)
        ngd.install(adb, self.profile, self.release, self.data_dir/'backups', args)
        return 'Installation prepared. Restart separately when ready. / Instalacja przygotowana. Uruchom ponownie, gdy będziesz gotowy.'

    def rollback(self, accepted=False):
        if not accepted:
            raise RuntimeError('Local rollback confirmation required / Wymagane potwierdzenie cofnięcia')
        self.checked = None
        adb = connect(self.exe, self.root)
        self._ensure_release(adb, restore=True)
        ngd.remove(adb, self.profile, self.release, SimpleNamespace(accept_oem_return=True))
        return 'OEM return prepared; restart separately. / Przygotowano powrót OEM; restart wykonaj osobno.'

    def restart(self, accepted=False):
        if not accepted:
            raise RuntimeError('Restart confirmation required / Wymagane potwierdzenie restartu')
        self.checked = None
        adb = connect(self.exe, self.root)
        identity = device_identity(adb)
        if adb.prop('ro.build.fingerprint') != self.profile['fingerprint']:
            raise RuntimeError('Wrong target / Niewłaściwa jednostka')
        self._ensure_release(adb, restore=adb.exists(ngd.DEVICE))
        before = adb.sh('cat /proc/sys/kernel/random/boot_id')
        adb.run('reboot')
        deadline = time.monotonic() + 180
        while time.monotonic() < deadline:
            time.sleep(3)
            try:
                adb = connect(self.exe, self.root)
                if device_identity(adb) != identity:
                    raise RuntimeError('Wrong device after reboot / Inne urządzenie po restarcie')
                if adb.prop('sys.boot_completed') != '1':
                    continue
                if adb.sh('cat /proc/sys/kernel/random/boot_id') == before:
                    continue
                manifest = ngd.verify_release(self.release, self.profile)
                service = adb.prop('init.svc.ngd-native-bt')
                on = adb.sh('settings get global bluetooth_on')
                code_path = adb.sh('pm path org.codeaurora.bluetooth')
                result = {'service': service, 'bluetooth_on': on, 'code_path': code_path,
                          'audio_listening_test_required': True}
                if adb.exists(ngd.DEVICE+'/enabled'):
                    adb.sh(ngd.DEVICE+'/ngd16_native_bt --verify-payload')
                    if (adb.sha(ngd.DEVICE+'/ngd16_native_bt') != manifest['files']['ngd16_native_bt'] or
                            adb.sha('/system/app/BluetoothExt/BluetoothExt.apk') != manifest['files']['BluetoothNGD.apk']):
                        raise RuntimeError('Post-boot payload mismatch / Niezgodne pliki po restarcie')
                    result['status'] = 'AUTOSTART_OK' if service == 'running' and on == '1' and code_path == 'package:/system/app/BluetoothExt/BluetoothExt.apk' else 'AUTOSTART_NOT_CONFIRMED'
                else:
                    result['status'] = 'OEM_RETURN_CHECK_REQUIRED'
                return result
            except (RuntimeError, subprocess.TimeoutExpired):
                # A reconnect is common because the radio returns USB to Host.
                # Keep waiting; never issue another reboot or modify payload here.
                continue
        raise RuntimeError('Reconnect USB in Device/ADB mode and check status. No further changes made. / Ustaw Device/ADB i sprawdź stan. Nie wykonano dalszych zmian.')


def find_adb():
    adjacent = Path(sys.executable).parent/'platform-tools/adb.exe'
    bundled = ngd.BASE/'platform-tools/adb.exe'
    if bundled.is_file():
        import ngd_components
        private = Path(os.environ.get('LOCALAPPDATA', str(Path.home())))/'NEXTGENDRIVE/BluetoothInstaller'
        return str(ngd_components.persistent_adb(private))
    return str(adjacent) if adjacent.is_file() else shutil.which('adb') or ''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true', help='Offline resource check; no ADB or UI')
    parser.add_argument('--diagnose', action='store_true', help='Read-only diagnostic (unless --root explicitly supplied)')
    parser.add_argument('--adb', default=find_adb())
    parser.add_argument('--release', type=Path, help='Optional expert override; default kit is prepared automatically')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--root', action='store_true', help='Explicitly request ADB root for diagnostic')
    parser.add_argument('--reconstruct-offline', action='store_true', help='Reconstruction test with owner OEM files; no ADB')
    parser.add_argument('--oem-inputs', type=Path, help='Directory with the five OEM inputs named by the bundle index')
    args = parser.parse_args()
    if args.self_test:
        ngd.load_profile()
        import ngd_components
        ngd_components.load_bundle()
        if getattr(sys, 'frozen', False):
            ngd_components.verify_adb()
        if os.name == 'nt':
            import tkinter
            from ngd_installer_ui import Window
            root = tkinter.Tk()
            root.withdraw()
            Window(root)
            root.update_idletasks()
            root.destroy()
        print('PUBLIC_INSTALLER_OFFLINE_SELF_TEST_PASS; NO_DEVICE_ACCESSED')
        return
    if args.reconstruct_offline:
        if not args.oem_inputs or not args.output:
            raise SystemExit('--oem-inputs and --output are required')
        import ngd_components
        bundle, index = ngd_components.load_bundle()
        inputs = {key:args.oem_inputs/key for key in index['oem_inputs']}
        ngd_components.assemble(bundle, index, inputs, args.output)
        if sys.stdout is not None:
            print('COMPLETE_RECONSTRUCTION_PASS; EXACT_REVIEWED_HASHES; NO_DEVICE_ACCESSED')
        return
    if args.diagnose:
        if not args.adb:
            raise SystemExit('Select adb with --adb')
        try:
            result = Installer(args.adb, args.release, Path.home()/'.ngd-bluetooth', root=args.root).check()
        except (RuntimeError, OSError, ValueError, subprocess.TimeoutExpired) as exc:
            result = {'schema': 1, 'installer': VERSION, 'status': 'NOT_READY', 'message': str(exc)}
        text = json.dumps(result, ensure_ascii=False, indent=2)+'\n'
        if args.output:
            with args.output.open('x', encoding='utf-8') as output:
                output.write(text)
        if sys.stdout is not None:
            print(text)
        return
    from ngd_installer_ui import launch
    launch()


if __name__ == '__main__':
    main()
