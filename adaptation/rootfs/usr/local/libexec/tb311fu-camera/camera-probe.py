#!/usr/bin/env python3
"""TB311FU vendor camera bring-up; identification is not image capture.

Run as root on the tablet. Serializes with display/codec maintenance.
The original 6.6 module dispatch was checked with Ghidra before using this ABI.
Never loads camera_isp_4_t before mdp_drv_mt6768: that dereferences NULL.
"""
import argparse
import ctypes as C
import fcntl
import hashlib
import json
import mmap
import os
from pathlib import Path
import platform
import subprocess


class Feature(C.Structure):
    _fields_ = [('camera', C.c_uint32), ('feature', C.c_uint32),
                ('data', C.c_void_p), ('length', C.c_void_p)]


class CamBuffer(C.Structure):
    _fields_ = [('fd', C.c_int32), ('iova', C.c_uint64),
                ('username', C.c_char * 64), ('secure', C.c_bool),
                ('secure_handle', C.c_uint32)]


def feature(fd, camera, number, size=4, payload=None):
    data = C.create_string_buffer(size) if payload is None else C.create_string_buffer(payload, size)
    length = C.c_uint32(size)
    arg = Feature(camera, number, C.addressof(data), C.addressof(length))
    fcntl.ioctl(fd, 0xc018690f, arg)
    return data.raw[:length.value]


def load(name):
    subprocess.run(['modprobe', name], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--isp', action='store_true', help='also open/close ISP')
    parser.add_argument('--buffers', action='store_true', help='also map/unmap DMA buffer')
    args = parser.parse_args()
    if os.geteuid() or platform.machine() != 'aarch64':
        raise SystemExit('Run as root on the aarch64 TB311FU')
    if platform.release() != '6.6.142-pmos-minimal+':
        raise SystemExit('Kernel ABI has not been verified')
    dt = Path('/sys/firmware/devicetree/base/model').read_bytes()
    if b'MT8786' not in dt:
        raise SystemExit(f'Unexpected platform: {dt!r}')
    with open('/run/lock/tb311fu-gpu-video', 'a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        module = Path('/lib/modules') / platform.release() / 'extra/mediatek/imgsensor_isp4_t.ko'
        if hashlib.sha256(module.read_bytes()).hexdigest() != '32b9d78f02d4dc943e903d72c4112c13455fdd8790c20d0e6fcc479526c329bc':
            raise SystemExit('Unverified sensor module')
        load('imgsensor_isp4_t')
        report = {'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
                  'kernel': platform.release(), 'capture_working': False, 'sensors': []}
        fd = os.open('/dev/kd_camera_hw', os.O_RDWR | os.O_CLOEXEC)
        try:
            for camera in (0, 1):
                index = int.from_bytes(feature(fd, camera, 0xc23), 'little', signed=True)
                item = {'camera': camera, 'driver_index': index}
                if index < 0:
                    raise RuntimeError(f'Sensor not detected: {item}')
                try:
                    feature(fd, camera, 0xc21)
                    item['open'] = True
                    sensor_id = feature(fd, camera, 0xbe1)
                    item['sensor_id'] = hex(int.from_bytes(sensor_id, 'little'))
                finally:
                    feature(fd, camera, 0xc22)
                report['sensors'].append(item)
                print(json.dumps(item), flush=True)
        finally:
            os.close(fd)
        if args.isp or args.buffers:
            load('mdp_drv_mt6768')
            load('camera_isp_4_t')
            isp = os.open('/dev/camera-isp', os.O_RDWR | os.O_CLOEXEC)
            try:
                with mmap.mmap(isp, 0x8000, flags=mmap.MAP_SHARED, prot=mmap.PROT_READ,
                               offset=0x1a040000) as regs:
                    report['seninf_top'] = regs[:32].hex()
            finally:
                os.close(isp)
            report['isp_open_close'] = True
        if args.buffers:
            load('camera_mem')
            heap = os.open('/dev/dma_heap/mtk_mm-uncached', os.O_RDWR | os.O_CLOEXEC)
            buf = C.create_string_buffer(24)
            import struct
            buf.raw = struct.pack('QIIQ', 8 * 1024 * 1024, 0, os.O_RDWR | os.O_CLOEXEC, 0)
            try:
                fcntl.ioctl(heap, 0xc0184800, buf)
            finally:
                os.close(heap)
            dmafd = struct.unpack('QIIQ', buf.raw)[1]
            cm = os.open('/dev/camera-mem', os.O_RDWR | os.O_CLOEXEC)
            node = CamBuffer(dmafd, 0, b'tb311fu-camera-probe', False, 0)
            mapped = False
            try:
                fcntl.ioctl(cm, 0xc0584900, node)
                mapped = True
                report['buffer_iova'] = hex(node.iova)
                if not node.iova:
                    raise RuntimeError('Empty camera DMA mapping')
            finally:
                if mapped:
                    fcntl.ioctl(cm, 0x40584901, node)
                os.close(cm)
                os.close(dmafd)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
