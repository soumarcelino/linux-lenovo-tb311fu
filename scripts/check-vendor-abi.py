#!/usr/bin/env python3
"""Check imported symbol CRCs against a built kernel, without altering modules."""
from pathlib import Path
import argparse,json,subprocess
p=argparse.ArgumentParser();p.add_argument('out',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parent.parent
symbols={v[1]:int(v[0],16) for line in (a.out/'Module.symvers').read_text().splitlines() if (v:=line.split())}
results={};count=0
for folder in ('vendor-input/modules','vendor-input/ramdisk/lib/modules'):
 for f in (root/folder).glob('*.ko'):
  bad=[];count+=1
  for line in subprocess.check_output(['modprobe','--show-modversions',str(f)],text=True).splitlines():
   crc,name=line.split()
   if name in symbols and int(crc,16)!=symbols[name]:bad.append(name)
  if bad: results[str(f.relative_to(root))]=bad
if not count: raise SystemExit('Place vendor modules in vendor-input/modules first')
a.report.parent.mkdir(parents=True,exist_ok=True)
report={'checked':count,'mismatched':results,'note':'Compares vendor imports with kernel export CRCs.'}
a.report.write_text(json.dumps(report,indent=2)+'\n')
print(f'{count} modules checked; {len(results)} have CRC mismatches')
raise SystemExit(bool(results))
