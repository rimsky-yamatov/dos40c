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


def volume_descriptor(total_sectors, root_lba, root_size, ptable_size, ptl, ptm):
    v = bytearray(SEC)
    v[0] = 1
    v[1:6] = b"CD001"
    v[6] = 1
    v[8:40] = pad(b"DOS40C", 32)                  # system id
    v[40:72] = pad(VOLID.encode(), 32)            # volume id
    v[80:88] = both32(total_sectors)
    v[120:124] = both16(1)                        # volume set size
    v[124:128] = both16(1)                        # sequence number
    v[128:132] = both16(SEC)                      # logical block size
    v[132:140] = both32(ptable_size)
    v[140:144] = struct.pack("<I", ptl)
    v[148:152] = struct.pack(">I", ptm)
    v[156:190] = dirrec(b"\0", root_lba, root_size, True)
    v[190:318] = pad(b"", 128)                    # volume set id
    v[318:446] = pad(b"DOS40C PORT", 128)         # publisher
    v[446:574] = pad(b"mkiso.py", 128)            # data preparer
    v[574:702] = pad(b"mkiso.py", 128)            # application
    for off in (813, 830):                        # creation, modification
        v[off:off + 17] = ltime()
    v[847:864] = b"0" * 16 + b"\0"                # expiration: unset
    v[864:881] = b"0" * 16 + b"\0"                # effective
    v[881] = 1
    return bytes(v)


def boot_record(catalog_lba):
    v = bytearray(SEC)
    v[0] = 0
    v[1:6] = b"CD001"
    v[6] = 1
    v[7:39] = pad(b"EL TORITO SPECIFICATION", 32, b"\0")
    v[71:75] = struct.pack("<I", catalog_lba)
    return bytes(v)


def terminator():
    v = bytearray(SEC)
    v[0] = 255
    v[1:6] = b"CD001"
    v[6] = 1
    return bytes(v)


def boot_catalog(image_lba):
    c = bytearray(SEC)
    c[0] = 1                                      # header id
    c[1] = 0                                      # platform: 80x86
    c[4:28] = pad(b"DOS40C", 24, b"\0")
    c[30], c[31] = 0x55, 0xAA
    csum = (-sum(struct.unpack("<16H", bytes(c[:32])))) & 0xFFFF
    c[28:30] = struct.pack("<H", csum)
    c[32] = 0x88                                  # bootable
    c[33] = 2                                     # 1.44MB floppy emulation
    c[34:36] = struct.pack("<H", 0)               # load segment (default 07C0)
    c[36] = 0                                     # system type
    c[38:40] = struct.pack("<H", 1)               # sector count
    c[40:44] = struct.pack("<I", image_lba)
    return bytes(c)


def iso_name(path):
    base = os.path.basename(path).upper()
    if "." not in base:
        base += "."
    return (base + ";1").replace(" ", "_")


def build(out, floppy, extras):
    files = [("DOS40C.IMG;1", open(floppy, "rb").read())]
    for p in extras:
        files.append((iso_name(p), open(p, "rb").read()))
    files.sort(key=lambda f: f[0])

    # fixed layout: 16 PVD, 17 boot record, 18 terminator, 19/20 path
    # tables, 21 root directory, 22 boot catalog, 23.. file data
    lba_root, lba_cat, lba = 21, 22, 23
    placed = []
    for name, data in files:
        placed.append((name, lba, data))
        lba += (len(data) + SEC - 1) // SEC
    total = lba
    image_lba = next(l for n, l, _ in placed if n == "DOS40C.IMG;1")

    root = bytearray()
    root += dirrec(b"\0", lba_root, SEC, True)
    root += dirrec(b"\1", lba_root, SEC, True)
    for name, l, data in placed:
        root += dirrec(name, l, len(data))
    assert len(root) <= SEC, "too many files for a single directory sector"
    root = bytes(root).ljust(SEC, b"\0")

    ptl = struct.pack("<BBIH", 1, 0, lba_root, 1) + b"\0" + b"\0"
    ptm = struct.pack(">BBIH", 1, 0, lba_root, 1) + b"\0" + b"\0"
    ptsize = len(ptl)

    img = bytearray(total * SEC)

    def put(lba_, data):
        img[lba_ * SEC:lba_ * SEC + len(data)] = data

    put(16, volume_descriptor(total, lba_root, SEC, ptsize, 19, 20))
    put(17, boot_record(lba_cat))
    put(18, terminator())
    put(19, ptl)
    put(20, ptm)
    put(lba_root, root)
    put(lba_cat, boot_catalog(image_lba))
    for _, l, data in placed:
        put(l, data)
    with open(out, "wb") as f:
        f.write(img)
    return total


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    n = build(sys.argv[1], sys.argv[2], sys.argv[3:])
    print("%s: %d sectors (%d bytes)" % (sys.argv[1], n, n * SEC))
