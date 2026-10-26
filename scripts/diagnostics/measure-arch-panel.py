#!/usr/bin/env python3
from pathlib import Path
import fcntl,struct,time,subprocess,re,statistics,collections
node=None
for p in Path('/dev/input').glob('event*'):
 with p.open('rb',buffering=0) as f:
  n=bytearray(128);fcntl.ioctl(f,0x80804506,n)
  if n.split(b'\0')[0]==b'himax-touchscreen':node=p
assert node
start=time.time()
with node.open('wb',buffering=0) as f:
 def emit(t,c,v):f.write(struct.pack('llHHi',0,0,t,c,v))
 for gesture in range(4):
  emit(3,47,0);emit(3,57,8000+gesture);emit(1,330,1)
  for step in range(51):
   y=20+step*270 if gesture%2==0 else 13520-step*270
   emit(3,53,6000);emit(3,54,y);emit(3,0,6000);emit(3,1,y);emit(0,0,0);time.sleep(.016)
  emit(3,57,-1);emit(1,330,0);emit(0,0,0);time.sleep(.4)
pid=subprocess.check_output(['pgrep','-x','plasmashell'],text=True).strip()
s=subprocess.check_output(['journalctl','-b','--since=@'+str(start),'--no-pager','_PID='+pid],text=True)
a=[int(v) for v in re.findall(r'frame rendered in (\d+)ms',s)]
print('frames',len(a),'median',statistics.median(a) if a else None,'over40',sum(v>40 for v in a),'max',max(a) if a else None)
print('\n'.join(x for x in s.splitlines() if 'frame rendered' in x)[-1800:])
