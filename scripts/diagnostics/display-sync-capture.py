#!/usr/bin/env python3
"""Measure a real Wayland fullscreen workload, scanout waits and KMS events.

Root on the TB311FU only. No timing results are inferred from FPS. Profiling
and tracing are disabled again in finally; the desktop stays in place.
"""
import json
import os
import signal
from pathlib import Path
import subprocess
import sys
import time

label = sys.argv[1]
seconds = int(sys.argv[2]) if len(sys.argv) > 2 else 35
if not label.replace('-', '').isalnum():
    raise SystemExit('Invalid label')
assert os.uname().release == '6.6.142-pmos-minimal+'
assert 'MT8786' in Path('/proc/device-tree/model').read_text()
root = Path(os.environ.get('TB311FU_CAPTURE_DIR', '/run/tb311fu-display-sync')) / label
root.mkdir(parents=True, exist_ok=False)
params = Path('/sys/module/tb311fu_scanout/parameters')
traced = '--untraced' not in sys.argv
trace = Path('/sys/kernel/debug/tracing/instances/tb311fu-display-sync')
trace.mkdir(exist_ok=False)
(trace / 'buffer_size_kb').write_text('4096')
(trace / 'trace_clock').write_text('mono')
for event in ('dma_fence/dma_fence_wait_start', 'dma_fence/dma_fence_wait_end',
              'drm/drm_vblank_event', 'drm/drm_vblank_event_queued',
              'drm/drm_vblank_event_delivered'):
    (trace / 'events' / event / 'enable').write_text('1' if traced else '0')
pid = subprocess.check_output(['pgrep', '-x', 'kwin_wayland'], text=True).split()[0]
env = dict(x.split('=', 1) for x in Path(f'/proc/{pid}/environ').read_text().split('\0') if '=' in x)
identity = Path(f'/proc/{pid}').stat()
user = ['setpriv', f'--reuid={identity.st_uid}', f'--regid={identity.st_gid}', '--init-groups', 'env']
user += [key + '=' + value for key, value in env.items()]
user += ['WAYLAND_DISPLAY=wayland-0']
read_params = lambda: {p.name: p.read_text().strip() for p in params.iterdir()}
boot = Path('/proc/sys/kernel/random/boot_id').read_text().strip()
result = {'boot_before': boot, 'before': read_params(), 'seconds': seconds, 'traced': traced}
workload = None
try:
    (params / 'profile').write_text('1')
    (trace / 'trace').write_text('')
    (trace / 'tracing_on').write_text('1')
    result['start_monotonic'] = time.monotonic()
    with (root / 'workload.log').open('w') as output:
        workload = subprocess.Popen(user + ['weston-simple-egl', '-f', '-v', '-o'],
                                    stdout=output, stderr=subprocess.STDOUT)
        time.sleep(seconds)
        result['workload_alive'] = workload.poll() is None
        result['after'] = read_params()
        result['end_monotonic'] = time.monotonic()
        result['workload_returncode_before_stop'] = workload.poll()
        workload.send_signal(signal.SIGINT)
        workload.wait(timeout=10)
finally:
    (trace / 'tracing_on').write_text('0')
    (params / 'profile').write_text('0')
    if workload and workload.poll() is None:
        workload.kill()
        workload.wait()
    (root / 'trace.txt').write_text((trace / 'trace').read_text())
    (root / 'trace-buffer.txt').write_text('\n'.join(
        p.read_text() for p in sorted(trace.glob('per_cpu/cpu*/stats'))))
    trace.rmdir()
result['boot_after'] = Path('/proc/sys/kernel/random/boot_id').read_text().strip()
result['kernel_log'] = subprocess.check_output(['dmesg'], text=True)
result['kms_state'] = Path('/sys/kernel/debug/dri/0/state').read_text()
(root / 'result.json').write_text(json.dumps(result, indent=2))
print(json.dumps({key: value for key, value in result.items() if key not in ('kernel_log', 'kms_state')}, indent=2))
print('Artifacts:', root)
