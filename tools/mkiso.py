#!/usr/bin/env python3
"""mkiso.py - build a bootable ISO9660 image (El Torito, 1.44MB floppy
emulation) from a FAT12 floppy image.  Pure Python, no external tools.

    mkiso.py OUT.ISO FLOPPY.IMG [EXTRA_FILE ...]

The floppy image is stored as DOS40C.IMG and registered as the boot
image; BIOSes expose it as drive A:, which is what the DOS40C boot
sector expects.  Timestamps are fixed for reproducible output.
"""
import os
import struct
import sys

SEC = 2048
VOLID = "DOS40C"
STAMP = (1988, 6, 17, 0, 0, 0)        # reproducible


def both16(v):
    return struct.pack("<H", v) + struct.pack(">H", v)


def both32(v):
    return struct.pack("<I", v) + struct.pack(">I", v)


def dstamp():
    y, mo, d, h, mi, s = STAMP
    return bytes([y - 1900, mo, d, h, mi, s, 0])


def ltime():
    y, mo, d, h, mi, s = STAMP
    return ("%04d%02d%02d%02d%02d%02d00" % (y, mo, d, h, mi, s)).encode() + b"\0"


def pad(b, n, fill=b" "):
    return b.ljust(n, fill)[:n]


def dirrec(name, lba, size, is_dir=False):
    ident = name if isinstance(name, bytes) else name.encode()
    n = len(ident)
    length = 33 + n + (1 if n % 2 == 0 else 0)
    r = bytearray()
    r += bytes([length, 0])
    r += both32(lba) + both32(size)
    r += dstamp()
    r += bytes([2 if is_dir else 0, 0, 0])
    r += both16(1)
    r += bytes([n]) + ident
    if n % 2 == 0:
        r += b"\0"
    assert len(r) == length
    return bytes(r)
