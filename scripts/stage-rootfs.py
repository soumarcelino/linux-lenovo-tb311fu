#!/usr/bin/env python3
"""Assemble hardware and Plasma Mobile configuration for a target account."""
import argparse
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--user', required=True)
p.add_argument('--group')
p.add_argument('--uid', type=int, required=True)
p.add_argument('--gid', type=int, required=True)
p.add_argument('--home')
p.add_argument('--output', type=Path, default=ROOT / 'build/rootfs')
a = p.parse_args()
if not re.fullmatch(r'[a-z_][a-z0-9_-]*', a.user):
    p.error('Use a Linux account name')
if a.uid < 1 or a.gid < 1:
    p.error('Use a non-root session account')
group = a.group or a.user
home = a.home or f'/home/{a.user}'
if not re.fullmatch(r'[a-z_][a-z0-9_-]*', group) or not re.fullmatch(r'/[A-Za-z0-9_./-]+', home):
    p.error('Use a Linux group name and an absolute home path without spaces')
out = a.output.resolve()
if out.exists():
    p.error('Choose a new output directory')
for src in (ROOT / 'adaptation/rootfs', ROOT / 'profiles/plasma-mobile/rootfs'):
    shutil.copytree(src, out, dirs_exist_ok=True)
values = {'USER': a.user, 'GROUP': group, 'UID': str(a.uid), 'HOME': home}
for path in out.rglob('*'):
    if path.is_file():
        text = path.read_text()
        for key, value in values.items():
            text = text.replace(f'@{key}@', value)
        path.write_text(text)
config = out / home.lstrip('/') / '.config'
shutil.copytree(ROOT / 'profiles/plasma-mobile/user-config', config, dirs_exist_ok=True)
print(f'Rootfs: {out}')
print(f'Account: {a.user}:{group} ({a.uid}:{a.gid}); home: {home}')
