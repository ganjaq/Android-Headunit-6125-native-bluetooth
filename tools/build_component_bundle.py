#!/usr/bin/env python3
"""Export project-only components and non-payload ZIP recipes from a reviewed kit."""
from pathlib import Path
import argparse
import json
import shutil
import struct
import tempfile
import zipfile

import ngd_components as components
import ngd_patch as ngd

OEM = {
    name: {'remote':'/system/lib64/'+name, 'payload':'lib/arm64/'+name}
    for name in ('libbluetooth-binder.so', 'libchrome.so', 'libevent.so', 'libnativehelper.so')
}
OEM['settings'] = {'remote':'/product/priv-app/Settings/Settings.apk',
                   'restore_remote':ngd.DEVICE+'/view/oem-settings/Settings.apk'}


def entry_range(raw, info):
    start = info.header_offset
    if raw[start:start+4] != b'PK\x03\x04':
        raise RuntimeError('Bad ZIP local header')
    names, extra = struct.unpack_from('<HH', raw, start+26)
    data = start+30+names+extra
    return data, data+info.compress_size


def recipe(apk, root, stem, settings=False):
    raw = Path(apk).read_bytes()
    blob, pieces = bytearray(), []
    def literal(data):
        if data:
            pieces.append({'literal':[len(blob), len(data)]})
            blob.extend(data)
    with zipfile.ZipFile(apk) as archive:
        cursor = 0
        for info in sorted(archive.infolist(), key=lambda i:i.header_offset):
            begin, end = entry_range(raw, info)
            if begin < cursor or end > len(raw):
                raise RuntimeError('Overlapping ZIP entries')
            literal(raw[cursor:begin])
            name = info.filename.rsplit('/', 1)[-1]
            if settings:
                if info.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
                    raise RuntimeError('Unknown Settings ZIP compression')
                part = {'input':'settings', 'entry':info.filename,
                        'encoding':'stored' if info.compress_type == zipfile.ZIP_STORED else 'deflate6',
                        'size':end-begin, 'sha256':components.sha(raw[begin:end])}
                if info.filename == 'classes.dex':
                    part['transform'] = 'settings-visibility'
                pieces.append(part)
            elif info.filename.startswith('lib/') and name in OEM and name != 'settings':
                if info.compress_type != zipfile.ZIP_STORED:
                    raise RuntimeError('OEM native library must be stored verbatim')
                pieces.append({'input':name, 'encoding':'stored', 'size':end-begin,
                               'sha256':components.sha(raw[begin:end])})
            else:
                literal(raw[begin:end])
            cursor = end
        literal(raw[cursor:])
    (root/(stem+'.blob')).write_bytes(blob)
    ngd.record(root/(stem+'.recipe.json'), {'schema':1, 'size':len(raw),
               'sha256':components.sha(raw), 'pieces':pieces})
    return {'literal_bytes':len(blob), 'rebuilt_bytes':len(raw), 'pieces':len(pieces)}


def build(kit, settings_original, output, index_output):
    kit, output, index_output = Path(kit), Path(output), Path(index_output)
    manifest = ngd.verify_release(kit)
    if manifest.get('variant') != 'avrcp-legacy-20261019':
        raise RuntimeError('Use the reviewed AVRCP kit')
    if output.exists() or index_output.exists():
        raise RuntimeError('Output/index exists; no overwrite')
    source = kit/'payload'
    inputs = {name:source/metadata['payload'] for name,metadata in OEM.items() if name != 'settings'}
    inputs['settings'] = Path(settings_original)
    if ngd.digest(inputs['settings']) != ngd.load_profile()['factory_files'][OEM['settings']['remote']]:
        raise RuntimeError('Wrong original Settings input')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.ngd-components-', dir=output.parent) as tmp:
        stage = Path(tmp)/'components'
        stage.mkdir()
        excluded = {'BluetoothNGD.apk', 'Settings.bluetooth-visible.apk'} | {v['payload'] for k,v in OEM.items() if k != 'settings'}
        project = [name for name in manifest['files'] if name not in excluded]
        for name in project:
            target = stage/'project'/name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source/name, target)
        metrics = {'bluetooth':recipe(source/'BluetoothNGD.apk', stage, 'bluetooth'),
                   'settings':recipe(source/'Settings.bluetooth-visible.apk', stage, 'settings', settings=True)}
        # This bundle must contain neither complete OEM native libraries nor any
        # Settings entry payload in literal segments. Settings literals are only
        # local ZIP headers/descriptors, alignment and signing/central metadata.
        for name in ('bluetooth', 'settings'):
            blob = (stage/(name+'.blob')).read_bytes()
            for key,path in inputs.items():
                if key != 'settings' and path.read_bytes() in blob:
                    raise RuntimeError('OEM library leaked into literal data')
        index = {'schema':1, 'variant':manifest['variant'], 'project_outputs':project,
                 'oem_inputs':{key:dict(details, sha256=ngd.digest(inputs[key])) for key,details in OEM.items()},
                 'files':{p.relative_to(stage).as_posix():ngd.digest(p) for p in sorted(stage.rglob('*')) if p.is_file()},
                 'recipe_metrics':metrics,
                 'provenance':'Project/AOSP binaries and ZIP metadata only. OEM payload is read locally from the owner radio. No signing private keys.'}
        # Strong check against real inputs: exact existing APK signatures/hashes,
        # unchanged controller and JNI; no new runtime build is introduced.
        assembled = components.assemble(stage, index, inputs, Path(tmp)/'verified-kit')
        if ngd.verify_release(assembled)['files'] != manifest['files']:
            raise RuntimeError('Real reconstruction differs from the reviewed kit')
        stage.rename(output)
    ngd.record(index_output, index)
    return index


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('kit', 'settings-original', 'out', 'index-output'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    result = build(args.kit, args.settings_original, args.out, args.index_output)
    print(json.dumps(result['recipe_metrics'], indent=2))
    print('PROJECT_ONLY_COMPONENTS_EXPORTED; REAL_RECONSTRUCTION_MATCHES_REVIEWED_KIT; NO_DEVICE_ACCESSED')
