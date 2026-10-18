#!/usr/bin/env python3
"""Assemble the verified local binary kit. No device/network access or data backups."""
from pathlib import Path
import hashlib,json,zipfile,sys,argparse
from ngd_patch import verify_release,load_profile

base=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--profile',type=Path);parser.add_argument('--release',type=Path,default=base/'release')
args=parser.parse_args(); profile=load_profile(args.profile); release=args.release.resolve()
manifest=verify_release(release,profile)
out=base/'dist';out.mkdir(exist_ok=True)
path=out/('nextgendrive-native-bt-'+manifest['version']+'-'+profile['id']+'-local.zip')
sources=[]
for name in ['README.md','README.en.md','LICENSE','NOTICE','CHANGELOG.md']:
    sources.append(base/name)
for folder in ['tools','profiles','docs','licenses','src','patches','tests','.agents/skills','apps']:
    sources.extend(p for p in (base/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc')
with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted((release/'payload').rglob('*')):
        if p.is_file(): z.write(p,'release/payload/'+p.relative_to(release/'payload').as_posix())
    z.write(release/'manifest.json','release/manifest.json')
    if args.profile: z.write(args.profile,'target-profile.json')
    for p in sorted(sources):
        rel=p.relative_to(base).as_posix()
        if any(part in rel.lower() for part in ['.snapshot','.receipt','baseline.json','btsnoop','logcat']):
            raise RuntimeError('Private data refused: '+rel)
        z.write(p,rel)
with zipfile.ZipFile(path,'r') as z:
    assert z.testzip() is None
sha=hashlib.file_digest(path.open('rb'),'sha256').hexdigest()
path.with_suffix('.zip.sha256').write_text(sha+'  '+path.name+'\n')
print(path); print('SHA256 '+sha)
