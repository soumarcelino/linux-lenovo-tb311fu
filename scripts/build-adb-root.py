#!/usr/bin/env python3
"""Build the pinned standalone AArch64 adbd with the TB311FU root service."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

root=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',type=Path,default=Path.home()/'Projects/standalone-linux-adbd')
parser.add_argument('--jobs',type=int,default=min(8,os.cpu_count() or 1))
args=parser.parse_args()
source=args.source.resolve()
spec=json.loads((root/'adaptation/src/adb/source.json').read_text())
def run(command,**kwargs):return subprocess.run(command,check=True,**kwargs)
if not source.exists():
 source.parent.mkdir(parents=True,exist_ok=True)
 run(['git','clone',spec['project']+'.git',str(source)])
 run(['git','checkout','--detach',spec['commit']],cwd=source)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==spec['commit'], 'Unexpected source revision'
build=source/'build/tb311fu-adbd'
env=dict(os.environ,CMAKE_TOOLCHAIN_FILE=str(source/'cmake/toolchains/aarch64-linux-gnu.cmake'),ANDROID_TOOLS_BUILD_JOBS=str(args.jobs))
# Upstream prepares and patches its pinned dependency submodules first.
run(['bash','scripts/build-linux-adbd.sh',str(build)],cwd=source,env=env)
vendor=source/'vendor/adb'
for patch_name in spec['patches']:
 patch=root/'adaptation/src/adb'/patch_name
 if subprocess.run(['git','apply','--reverse','--check',str(patch)],cwd=vendor,capture_output=True).returncode:
  run(['git','apply','--check',str(patch)],cwd=vendor)
  run(['git','apply',str(patch)],cwd=vendor)
run(['cmake','--build',str(build),'--target','adbd','-j',str(args.jobs)],cwd=source)
dest=root/'dist/adb-root';dest.mkdir(parents=True,exist_ok=True)
run(['aarch64-linux-gnu-strip','--strip-unneeded','-o',str(dest/'adbd'),str(build/'vendor/adbd')])
spec.update(binary_sha256=hashlib.sha256((dest/'adbd').read_bytes()).hexdigest(),
 patch_sha256={name:hashlib.sha256((root/'adaptation/src/adb'/name).read_bytes()).hexdigest() for name in spec['patches']},
 compiler=subprocess.check_output(['aarch64-linux-gnu-g++','--version'],text=True).splitlines()[0],
 submodules=subprocess.check_output(['git','submodule','status'],cwd=source,text=True).splitlines())
(dest/'manifest.json').write_text(json.dumps(spec,indent=2)+'\n')
print('Built root USB adbd:',spec['binary_sha256'])
