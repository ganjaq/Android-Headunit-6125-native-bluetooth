#!/usr/bin/env python3
"""Generate a reviewable UTF-8 patch; never edits the upstream tree."""
from pathlib import Path
import argparse, difflib

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--upstream',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args(); upstream=a.upstream.resolve()
    source=Path(__file__).resolve().parents[1]/'src/android-app'
    if not (upstream/'AndroidManifest.xml').is_file():
        raise SystemExit('Expected clean AOSP packages/apps/Bluetooth directory')
    changes=[]
    for path in sorted(source.rglob('*')):
        if not path.is_file():continue
        rel=path.relative_to(source).as_posix(); before=upstream/rel
        new=path.read_bytes(); old=before.read_bytes() if before.is_file() else b''
        if old==new:continue
        try: left=old.decode('utf-8');right=new.decode('utf-8')
        except UnicodeDecodeError:
            raise SystemExit('Binary source delta requires explicit review: '+rel)
        # Normalize source line endings; retain source contents and license notices.
        left=left.replace('\r\n','\n');right=right.replace('\r\n','\n')
        if left==right:continue
        changes.extend(difflib.unified_diff(left.splitlines(keepends=True),right.splitlines(keepends=True),
            fromfile='a/'+rel if before.is_file() else '/dev/null',tofile='b/'+rel))
    if a.output.exists():raise SystemExit('Refusing to overwrite existing patch')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_bytes(''.join(changes).encode('utf-8'))
    print(str(a.output.resolve()))

if __name__=='__main__':main()
