#!/usr/bin/env python3
"""Assemble the reviewed AVRCP kit offline from locally held artifacts. No ADB."""
from pathlib import Path
import argparse, json, shutil, tempfile
from ngd_patch import BASE, PROFILE, digest, verify_release


def assemble(base, apk, controller, bridge, out):
    manifest = verify_release(base)
    if manifest.get('variant') is not None:
        raise RuntimeError('STOP: expected the baseline 0.1.0 release')
    reviewed = json.loads((BASE/'profiles/payload-avrcp-20261019.json').read_text(encoding='utf-8'))
    replacements = {'BluetoothNGD.apk': Path(apk), 'ngd16_native_bt': Path(controller),
                    'lib/arm64/libngd_avrcp_legacy.so': Path(bridge)}
    for name, path in replacements.items():
        if digest(path) != reviewed['files'][name]:
            raise RuntimeError('STOP: unreviewed artifact: '+name)
    out = Path(out).resolve()
    if out.exists():
        raise RuntimeError('STOP: output exists; no overwrite')
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.ngd-avrcp-', dir=out.parent) as tmp:
        stage = Path(tmp)/'kit'
        shutil.copytree(Path(base)/'payload', stage/'payload')
        for name, path in replacements.items():
            shutil.copy2(path, stage/'payload'/name)
        (stage/'manifest.json').write_text(json.dumps(reviewed, indent=2)+'\n', encoding='utf-8')
        verify_release(stage)
        stage.rename(out)
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('base-release', 'apk', 'controller', 'bridge', 'out'):
        p.add_argument('--'+name, type=Path, required=True)
    a = p.parse_args()
    print('OFFLINE_KIT_VERIFIED '+str(assemble(a.base_release, a.apk, a.controller, a.bridge, a.out)))


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError) as exc:
        raise SystemExit(str(exc))
