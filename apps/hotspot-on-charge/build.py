#!/usr/bin/env python3
"""Build this companion against an existing Android10 AOSP tree; no downloads/device actions."""
from pathlib import Path
import argparse,hashlib,os,subprocess,tempfile,zipfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--aosp',type=Path,required=True);p.add_argument('--keystore',type=Path,required=True)
p.add_argument('--alias',default='ngd-dev');p.add_argument('--out',type=Path,required=True)
a=p.parse_args();a.out=a.out.resolve()
if a.out.exists():raise SystemExit('Refusing existing output')
if not os.environ.get('NGD_HOTSPOT_KEYSTORE_PASSWORD'):raise SystemExit('Set NGD_HOTSPOT_KEYSTORE_PASSWORD locally; never commit a key/password')
s=Path(__file__).resolve().parent/'android';b=a.aosp.resolve()
jdk=b/'prebuilts/jdk/jdk9/linux-x86/bin';sdk=b/'prebuilts/sdk/tools/linux';android=b/'prebuilts/sdk/29/system/android.jar'
def run(args):subprocess.run([str(x) for x in args],check=True)
with tempfile.TemporaryDirectory(prefix='ngd-hotspot-build-') as temp:
 w=Path(temp);(w/'classes').mkdir();(w/'dex').mkdir()
 run([sdk/'bin/aapt2','compile','--dir',s/'res','-o',w/'resources.zip'])
 run([sdk/'bin/aapt2','link','--manifest',s/'AndroidManifest.xml','-I',android,'--java',w/'generated','-o',w/'resources.apk',w/'resources.zip'])
 run([jdk/'javac','-source','8','-target','8','-bootclasspath',str(b/'prebuilts/sdk/tools/core-lambda-stubs.jar')+':'+str(android),'-d',w/'classes',*sorted((s/'src').rglob('*.java')),*sorted((w/'generated').rglob('*.java'))])
 with zipfile.ZipFile(w/'classes.jar','w') as z:
  for f in (w/'classes').rglob('*.class'):z.write(f,f.relative_to(w/'classes').as_posix())
 r8=next(f for f in (b/'prebuilts').rglob('*.jar') if f.name in ('r8.jar','d8.jar'))
 run([jdk/'java','-cp',r8,'com.android.tools.r8.D8','--min-api','26','--release','--lib',android,'--output',w/'dex',w/'classes.jar'])
 with zipfile.ZipFile(w/'resources.apk') as src,zipfile.ZipFile(w/'unsigned.apk','w') as z:
  for info in src.infolist():z.writestr(info,src.read(info.filename))
  for f in (w/'dex').glob('classes*.dex'):z.write(f,f.name)
 run([sdk/'bin/zipalign','-f','-p','4',w/'unsigned.apk',w/'aligned.apk'])
 a.out.parent.mkdir(parents=True,exist_ok=True)
 run([jdk/'java','-jar',sdk/'lib/apksigner.jar','sign','--ks',a.keystore,'--ks-key-alias',a.alias,'--ks-pass','env:NGD_HOTSPOT_KEYSTORE_PASSWORD','--key-pass','env:NGD_HOTSPOT_KEYSTORE_PASSWORD','--out',a.out,w/'aligned.apk'])
 run([jdk/'java','-jar',sdk/'lib/apksigner.jar','verify','--verbose',a.out])
print('BUILD_PASS',hashlib.sha256(a.out.read_bytes()).hexdigest())