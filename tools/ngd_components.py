"""Reconstruct reviewed APKs locally; OEM payload is never bundled or uploaded."""
from pathlib import Path
from contextlib import ExitStack
import hashlib
import json
import shutil
import struct
import tempfile
import uuid
import zipfile
import zlib

import ngd_patch as ngd

INDEX = 'public-components-20261020.json'


def verify_adb(root=None):
    root = Path(root) if root is not None else ngd.BASE/'platform-tools'
    index = json.loads((ngd.BASE/'profiles/public-adb-3701.json').read_text(encoding='utf-8'))
    for name, expected in index['files'].items():
        if ngd.digest(safe_file(root, name)) != expected:
            raise RuntimeError('Bundled ADB checksum mismatch: '+name)
    return root/'adb.exe'


def persistent_adb(data_dir):
    """Do not start the background ADB server from a onefile temporary folder."""
    source = ngd.BASE/'platform-tools'
    verify_adb(source)
    parent = Path(data_dir)/'tools'
    target = parent/'adb-37.0.1'
    if target.exists():
        return verify_adb(target)
    parent.mkdir(parents=True, exist_ok=True)
    index = json.loads((ngd.BASE/'profiles/public-adb-3701.json').read_text(encoding='utf-8'))
    with tempfile.TemporaryDirectory(prefix='.adb-stage-', dir=parent) as tmp:
        stage = Path(tmp)/'adb'
        stage.mkdir()
        for name in index['files']:
            shutil.copyfile(safe_file(source, name), stage/name)
        verify_adb(stage)
        try:
            stage.rename(target)
        except FileExistsError:
            pass
    return verify_adb(target)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def safe_file(root, relative):
    if not isinstance(relative, str) or not relative or '\\' in relative:
        raise RuntimeError('Invalid component path')
    path = Path(root)/relative
    if path.is_symlink() or Path(root).resolve() not in path.resolve().parents:
        raise RuntimeError('Component path escape/symlink')
    return path


def load_bundle(root=None):
    root = Path(root) if root is not None else ngd.BASE/'installer/components'
    index = json.loads((ngd.BASE/'profiles'/INDEX).read_text(encoding='utf-8'))
    actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()}
    if actual != set(index['files']):
        raise RuntimeError('Incomplete bundled components / Niekompletne składniki instalatora')
    for name, expected in index['files'].items():
        if ngd.digest(safe_file(root, name)) != expected:
            raise RuntimeError('Bundled component checksum mismatch: '+name)
    return root, index


def visibility_dex(data):
    data = bytearray(data)
    offset = 2649532
    if (data[:4] != b'dex\n' or len(data) < offset+20 or
            struct.unpack_from('<I', data, offset+12)[0] != 2 or
            data[offset+16:offset+20] != bytes.fromhex('12000f00')):
        raise RuntimeError('Unexpected original Settings bytecode')
    data[offset+17] = 0x10
    data[12:32] = hashlib.sha1(data[32:]).digest()
    struct.pack_into('<I', data, 8, zlib.adler32(data[12:]) & 0xffffffff)
    return bytes(data)


def reconstruct(recipe_path, blob_path, inputs, output):
    recipe = json.loads(Path(recipe_path).read_text(encoding='utf-8'))
    if recipe['schema'] != 1 or not 0 < recipe['size'] <= 64*1024*1024:
        raise RuntimeError('Invalid reconstruction recipe')
    blob = Path(blob_path).read_bytes()
    written = 0
    digest = hashlib.sha256()
    with ExitStack() as stack:
        archives = {}
        target = stack.enter_context(Path(output).open('xb'))
        for piece in recipe['pieces']:
            if set(piece) == {'literal'}:
                start, length = piece['literal']
                if not (isinstance(start, int) and isinstance(length, int) and start >= 0 and length > 0 and start+length <= len(blob)):
                    raise RuntimeError('Invalid literal bounds')
                data = blob[start:start+length]
            else:
                source = piece['input']
                if source not in inputs:
                    raise RuntimeError('Required OEM input missing: '+source)
                if 'entry' in piece:
                    if source not in archives:
                        archives[source] = stack.enter_context(zipfile.ZipFile(inputs[source]))
                    info = archives[source].getinfo(piece['entry'])
                    if info.file_size > 48*1024*1024:
                        raise RuntimeError('Oversized original ZIP entry')
                    data = archives[source].read(info)
                else:
                    data = Path(inputs[source]).read_bytes()
                if piece.get('transform') == 'settings-visibility':
                    if source != 'settings' or piece.get('entry') != 'classes.dex':
                        raise RuntimeError('Unexpected bytecode transformation target')
                    data = visibility_dex(data)
                elif piece.get('transform') is not None:
                    raise RuntimeError('Unknown bytecode transformation')
                if piece['encoding'] == 'deflate6':
                    compressor = zlib.compressobj(6, zlib.DEFLATED, -15)
                    data = compressor.compress(data) + compressor.flush()
                elif piece['encoding'] != 'stored':
                    raise RuntimeError('Unknown ZIP encoding')
                if len(data) != piece['size'] or sha(data) != piece['sha256']:
                    raise RuntimeError('OEM reconstruction differs: '+source+' '+piece.get('entry', ''))
            written += len(data)
            if written > recipe['size']:
                raise RuntimeError('Reconstruction exceeds reviewed size')
            target.write(data)
            digest.update(data)
    if written != recipe['size'] or digest.hexdigest() != recipe['sha256']:
        raise RuntimeError('Reconstructed APK checksum mismatch')
    return Path(output)


def assemble(bundle, index, inputs, output):
    """Offline only. Preserve original input files; publish no partial kit."""
    output = Path(output).resolve()
    if output.exists():
        raise RuntimeError('Prepared kit exists; no overwrite')
    for key, details in index['oem_inputs'].items():
        if key not in inputs or ngd.digest(inputs[key]) != details['sha256']:
            raise RuntimeError('Original OEM input checksum mismatch: '+key)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.ngd-assemble-', dir=output.parent) as tmp:
        stage = Path(tmp)/'kit'
        payload = stage/'payload'
        payload.mkdir(parents=True)
        for name in index['project_outputs']:
            target = safe_file(payload, name)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(safe_file(bundle, 'project/'+name), target)
        for key, details in index['oem_inputs'].items():
            if details.get('payload'):
                target = safe_file(payload, details['payload'])
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(inputs[key], target)
        for name, stem in [('BluetoothNGD.apk', 'bluetooth'), ('Settings.bluetooth-visible.apk', 'settings')]:
            reconstruct(safe_file(bundle, stem+'.recipe.json'), safe_file(bundle, stem+'.blob'), inputs, payload/name)
        manifest = json.loads((ngd.BASE/'profiles/payload-avrcp-20261019.json').read_text(encoding='utf-8'))
        ngd.record(stage/'manifest.json', manifest)
        ngd.verify_release(stage)
        stage.rename(output)
    return output


def prepare(adb, profile, data_dir, identity, restore=False, bundle=None):
    """Only ADB reads/pulls and host writes. Device activation is separate."""
    if profile != ngd.load_profile():
        raise RuntimeError('Automatic preparation requires the reviewed reference profile; twins need their reviewed kit')
    bundle, index = load_bundle(bundle)
    if adb.sh('id -u') != '0' or adb.prop('ro.build.fingerprint') != profile['fingerprint']:
        raise RuntimeError('Wrong target or missing ADB root')
    if restore:
        baseline = json.loads(adb.sh('cat '+ngd.DEVICE+'/distribution-baseline.json'))
        if baseline['profile'] != profile['id'] or baseline['factory_hashes'] != profile['factory_files']:
            raise RuntimeError('Saved device baseline differs')
        manifest = json.loads((ngd.BASE/'profiles/payload-avrcp-20261019.json').read_text(encoding='utf-8'))
        if (adb.sha(ngd.DEVICE+'/ngd16_native_bt') != manifest['files']['ngd16_native_bt'] or
                adb.sha(ngd.INIT) != manifest['files']['ngd-native-bt.rc']):
            raise RuntimeError('Installed controller/init differ; individual rollback review required')
    else:
        baseline = ngd.inspect(adb, profile)
    work = Path(data_dir)/'prepared'/identity/uuid.uuid4().hex
    work.mkdir(parents=True, exist_ok=False)
    originals = work/'oem-inputs'
    originals.mkdir()
    inputs = {}
    print('Preparing complete kit locally; reading five OEM files / Przygotowanie paczki; odczyt pięciu plików OEM')
    for key, details in index['oem_inputs'].items():
        remote = details['restore_remote'] if restore and 'restore_remote' in details else details['remote']
        if adb.sha(remote) != details['sha256']:
            raise RuntimeError('OEM file on radio is incompatible: '+remote)
        target = originals/key
        adb.run('pull', remote, str(target), timeout=120)
        if ngd.digest(target) != details['sha256'] or adb.sha(remote) != details['sha256']:
            raise RuntimeError('OEM pull checksum mismatch: '+key)
        inputs[key] = target
    if not restore:
        after = ngd.inspect(adb, profile)
        if {k:v for k,v in baseline.items() if k != 'package'} != {k:v for k,v in after.items() if k != 'package'}:
            raise RuntimeError('Device state changed during local preparation')
    kit = assemble(bundle, index, inputs, work/'kit')
    ngd.record(work/'preparation.json', {'schema':1, 'device_identity_sha256':identity,
               'manifest_sha256':ngd.digest(kit/'manifest.json'), 'device_writes':False,
               'oem_inputs':{k:ngd.digest(p) for k,p in inputs.items()}})
    print('Complete kit verified; original tested APK/controller/JNI hashes retained / Pełna paczka zweryfikowana')
    return kit
