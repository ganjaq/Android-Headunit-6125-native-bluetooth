#!/usr/bin/env python3
"""Read-only twin assessment and offline per-target kit preparation. Never installs."""
from pathlib import Path
import argparse, copy, json, shutil
from ngd_patch import (BASE, PROFILE, Adb, digest, inspect, load_profile,
                       profile_digest, record, validate_twin_profile, verify_release)


def assess(adb):
    ref = load_profile()
    fp = adb.prop('ro.build.fingerprint')
    observed = {'fingerprint': fp, 'platform': adb.prop('ro.board.platform'),
                'controller_soc_properties': {k: adb.prop(k) for k in ('qcom.bluetooth.soc', 'vendor.qcom.bluetooth.soc')}}
    differences, unknown = [], []
    if observed['platform'] != 'trinket':
        unknown.append('Controller/SoC family requires evidence: '+repr(observed['platform']))
    soc_values = {v.lower() for v in observed['controller_soc_properties'].values() if v}
    if soc_values != {'cherokee'}:
        unknown.append('Qualcomm Cherokee identity missing or contradictory; inspect HCI/vendor evidence via the skill')
    hashes = {}
    for path, expected in ref['factory_files'].items():
        try:
            hashes[path] = adb.sha(path)
            if hashes[path] != expected:
                differences.append({'component': path, 'expected': expected, 'observed': hashes[path],
                                    'meaning': 'Needs ABI/integration port review; not proof of hardware incompatibility'})
        except (RuntimeError, OSError) as exc:
            unknown.append(path+': '+str(exc))
    candidate = copy.deepcopy(ref)
    candidate['fingerprint'] = fp
    # Recheck every existing safety condition against this observed fingerprint.
    try:
        inspect(adb, candidate)
    except (RuntimeError, OSError) as exc:
        unknown.append(str(exc))
    return {'schema': 1, 'reference': PROFILE, 'observation': observed,
            'factory_hashes': hashes, 'component_differences': differences, 'blockers': unknown,
            'status': 'TECHNICAL_CANDIDATE' if not differences and not unknown else 'NEEDS_EVIDENCE',
            'hardware_tested': False,
            'scope': 'Same reviewed Bluetooth/MCU/audio integration; different unrelated firmware allowed. No device writes.'}


def prepare(report, target_id, out):
    if report.get('schema') != 1 or report.get('reference') != PROFILE or report.get('status') != 'TECHNICAL_CANDIDATE':
        raise RuntimeError('STOP: a complete read-only technical candidate assessment is required')
    ref = load_profile()
    if (report.get('factory_hashes') != ref['factory_files'] or report.get('blockers') or
            report.get('component_differences') or report.get('observation', {}).get('platform') != 'trinket' or
            {v.lower() for v in report.get('observation', {}).get('controller_soc_properties', {}).values() if v} != {'cherokee'}):
        raise RuntimeError('STOP: assessment evidence inconsistent')
    profile = copy.deepcopy(ref)
    profile.update(id=target_id, fingerprint=report['observation']['fingerprint'], twin_family=PROFILE,
                   hardware_evidence={'units_tested': 0, 'status': 'TECHNICAL_CANDIDATE',
                                      'redistributable_installer_hardware_tested': False})
    validate_twin_profile(profile, ref)
    out = Path(out).resolve()
    if out.exists(): raise RuntimeError('STOP: target workspace exists; no overwrite')
    out.mkdir(parents=True)
    shutil.copytree(BASE/'src', out/'src')
    # Twin kits retain the reviewed 0.1.0 APK. Do not accidentally compile a
    # controller pinned to the newer, separately tested AVRCP APK/bridge.
    for relative in ('session/ngd12_artifact_digests.h', 'android-app/jni/ngd12_artifact_digests.h'):
        path = out/'src'/relative
        text = path.read_text(encoding='utf-8')
        text = '\n'.join(line for line in text.splitlines() if 'NGD_AVRCP_BRIDGE_SHA' not in line)
        import re
        text = re.sub(r'(#define NGD12_APK_SHA )"[a-f0-9]{64}"',
                      r'\1"23df2f6c0da3a649d03716def7c37fc0aab4944f16b0d23106c83c1b45919e90"', text)
        path.write_text(text+'\n', encoding='utf-8')
    for relative in ('session/ngd_native_session_main.cpp', 'installer/init_installer.cpp',
                     'android-app/jni/ngd_native_session_main.cpp', 'android-app/jni/ngd16_init_installer.cpp'):
        path = out/'src'/relative
        text = path.read_text(encoding='utf-8')
        if ref['fingerprint'] not in text: raise RuntimeError('STOP: target source marker missing')
        text = text.replace(ref['fingerprint'], profile['fingerprint'])
        if 'NGD_TARGET_FINGERPRINT=' not in text:
            token = 'int main(int argc,char** argv) {'
            if token not in text: raise RuntimeError('STOP: native source entry point changed')
            text = text.replace(token, token+'\n if(argc==2&&!strcmp(argv[1],\"--print-target\")){std::puts(\"NGD_TARGET_FINGERPRINT='+profile['fingerprint']+'\");return 0;}')
        path.write_text(text, encoding='utf-8')
    record(out/'target_profile.json', profile)
    record(out/'assessment.json', report)
    record(out/'source-manifest.json', {p.relative_to(out).as_posix(): digest(p)
                                      for p in sorted((out/'src').rglob('*')) if p.is_file()})
    (out/'BUILD_TARGET.md').write_text(
        '# Per-target native rebuild required\n\n'
        'Build src/session as ngd16_native_bt and src/installer/init_installer.cpp as ngd16_init_installer '
        'with the same reviewed AOSP Android10 ARM64 toolchain. Follow repository docs/BUILD.md. '
        'Do not rebuild or replace APK/JNI/audio payloads. No automatic AOSP download.\n\n'
        'Both binaries must report --print-target for this fingerprint. Review source-manifest.json '
        'and source diff before sealing. The source marker is a build mix-up check, not a signature or proof of ABI.\n\n'
        'Then use tools/ngd_twins.py seal --workspace HERE --controller FILE --installer FILE '
        '--base-release BASE_RELEASE --out NEW_KIT --ack-reviewed-build.\n', encoding='utf-8')
    return profile


def check_target_elf(path, fingerprint):
    data = Path(path).read_bytes()
    marker = ('NGD_TARGET_FINGERPRINT='+fingerprint).encode()+b'\0'
    if data[:6] != b'\x7fELF\x02\x01' or data[18:20] != b'\xb7\x00' or marker not in data:
        raise RuntimeError('STOP: target-specific ARM64 rebuild required: '+str(path))


def seal(workspace, controller, installer, base_release, out, acknowledged=False):
    if not acknowledged: raise RuntimeError('STOP: --ack-reviewed-build required; offline build review is not hardware validation')
    workspace, out = Path(workspace), Path(out).resolve()
    profile = load_profile(workspace/'target_profile.json')
    inventory = json.loads((workspace/'source-manifest.json').read_text(encoding='utf-8'))
    actual = {p.relative_to(workspace).as_posix(): digest(p) for p in (workspace/'src').rglob('*') if p.is_file()}
    if actual != inventory: raise RuntimeError('STOP: prepared source changed; review/regenerate workspace')
    base_manifest = verify_release(base_release)
    if base_manifest.get('variant') is not None:
        raise RuntimeError('STOP: twin workflow currently requires the baseline 0.1.0 payload')
    for path in (controller, installer): check_target_elf(path, profile['fingerprint'])
    if out.exists(): raise RuntimeError('STOP: kit exists; no overwrite')
    out.mkdir(parents=True)
    shutil.copytree(Path(base_release)/'payload', out/'payload')
    for path, name in ((controller, 'ngd16_native_bt'), (installer, 'ngd16_init_installer')):
        shutil.copy2(path, out/'payload'/name)
    files = {p.relative_to(out/'payload').as_posix(): digest(p) for p in (out/'payload').rglob('*') if p.is_file()}
    profile['release_files'] = files
    record(out/'profile.json', profile)
    record(out/'manifest.json', {'version': base_manifest['version']+'-twin-candidate', 'profile': profile['id'],
                                'target_profile_sha256': profile_digest(profile), 'files': files,
                                'hardware_verified': False})
    verify_release(out, profile)
    return profile


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='action', required=True)
    a = sub.add_parser('assess'); a.add_argument('--adb', default='adb'); a.add_argument('--serial'); a.add_argument('--output', type=Path, required=True)
    a = sub.add_parser('prepare'); a.add_argument('--assessment', type=Path, required=True); a.add_argument('--id', required=True); a.add_argument('--out', type=Path, required=True)
    a = sub.add_parser('seal')
    for name in ('workspace', 'controller', 'installer', 'base-release', 'out'): a.add_argument('--'+name, type=Path, required=True)
    a.add_argument('--ack-reviewed-build', action='store_true')
    args = p.parse_args(argv)
    if args.action == 'assess':
        if args.output.exists(): raise RuntimeError('STOP: assessment exists; no overwrite')
        report = assess(Adb(args.adb, args.serial)); record(args.output, report); print(report['status'])
    elif args.action == 'prepare':
        profile = prepare(json.loads(args.assessment.read_text(encoding='utf-8')), args.id, args.out)
        print('TARGET_SOURCES_PREPARED '+profile['id']+'; NATIVE_REBUILD_REQUIRED; NO_DEVICE_WRITES')
    else:
        profile = seal(args.workspace, args.controller, args.installer, args.base_release, args.out, args.ack_reviewed_build)
        print('TWIN_CANDIDATE_KIT '+profile['id']+'; HARDWARE_NOT_TESTED')


if __name__ == '__main__':
    try: main()
    except (RuntimeError, OSError, ValueError, KeyError) as exc: raise SystemExit('STOP: '+str(exc))
