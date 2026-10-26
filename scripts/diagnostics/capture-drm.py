#!/usr/bin/env python3
"""Read the current linear DRM scanout as PPM (root; no display changes)."""
import ctypes as c
import fcntl
import mmap
import os
from pathlib import Path
import re
import struct
import sys

u32=c.c_uint32
class Framebuffer(c.Structure):
    _fields_=[(name,u32) for name in ('fb_id','width','height','pitch','bpp','depth','handle')]
drm=c.CDLL('libdrm.so.2',use_errno=True)
drm.drmModeGetFB.argtypes=[c.c_int,u32]
drm.drmModeGetFB.restype=c.POINTER(Framebuffer)
card=Path('/dev/dri/by-path/platform-14000000.dispsys_config-card').resolve()
fd=os.open(card,os.O_RDWR)
# This vendor driver's legacy GETCRTC buffer_id can retain the initial black
# modeset buffer. The atomic plane state reports the actual current scanout.
state=Path('/sys/kernel/debug/dri',card.name.removeprefix('card'),'state').read_text()
active=re.search(r'\tcrtc=crtc-0\n\tfb=(\d+)',state)
assert active,'No active framebuffer'
fbid=int(active[1])
pointer=drm.drmModeGetFB(fd,fbid)
assert pointer,'GETFB failed'
fb=pointer.contents
assert fb.handle and fb.bpp==32,(fb.handle,fb.bpp)
request=bytearray(struct.pack('IIQ',fb.handle,0,0))
fcntl.ioctl(fd,0xc01064b3,request,True)
offset=struct.unpack_from('Q',request,8)[0]
with mmap.mmap(fd,fb.pitch*fb.height,flags=mmap.MAP_SHARED,prot=mmap.PROT_READ,offset=offset) as mapping:
    rgb=bytearray(fb.width*fb.height*3)
    for y in range(fb.height):
        row=mapping[y*fb.pitch:y*fb.pitch+fb.width*4]
        start=y*fb.width*3;end=start+fb.width*3
        rgb[start:end:3]=row[2::4]
        rgb[start+1:end:3]=row[1::4]
        rgb[start+2:end:3]=row[0::4]
sys.stdout.buffer.write(f'P6\n{fb.width} {fb.height}\n255\n'.encode()+rgb)
