#!/usr/bin/env python3
"""iso_check.py ISO FLOPPY.IMG - independent structural check of the ISO.

Parses ISO9660 + El Torito like a BIOS would: finds the boot record at
sector 17, follows the catalog, validates its checksum and media type,
extracts the boot image by LBA, and compares it with FLOPPY.IMG.
Exits 0 on success."""
import struct
import sys

SEC = 2048


def fail(msg):
    print("FAIL:", msg)
    sys.exit(1)


def main():
    iso = open(sys.argv[1], "rb").read()
    floppy = open(sys.argv[2], "rb").read()
    if len(iso) % SEC:
        fail("size is not a multiple of 2048")

    pvd = iso[16 * SEC:17 * SEC]
    if pvd[0] != 1 or pvd[1:6] != b"CD001" or pvd[6] != 1:
        fail("bad primary volume descriptor")
    total = struct.unpack_from("<I", pvd, 80)[0]
    if total * SEC != len(iso):
        fail("volume space size %d != file sectors %d" % (total, len(iso) // SEC))
    if struct.unpack_from("<H", pvd, 128)[0] != SEC:
        fail("logical block size")
    print("ok   PVD, volume id %r, %d sectors" % (pvd[40:72].decode().strip(), total))

    br = iso[17 * SEC:18 * SEC]
    if br[0] != 0 or br[1:6] != b"CD001" or not br[7:].startswith(b"EL TORITO SPECIFICATION"):
        fail("missing El Torito boot record at sector 17")
    if iso[18 * SEC] != 255:
        fail("missing volume descriptor terminator")
    cat_lba = struct.unpack_from("<I", br, 71)[0]
    cat = iso[cat_lba * SEC:cat_lba * SEC + 64]
    if cat[0] != 1 or cat[30:32] != b"\x55\xaa":
        fail("bad validation entry")
    if sum(struct.unpack("<16H", cat[:32])) & 0xFFFF:
        fail("validation entry checksum")
    if cat[32] != 0x88:
        fail("default entry is not bootable")
    media = cat[33]
    if media != 2:
        fail("media type %d, expected 2 (1.44MB floppy)" % media)
    img_lba = struct.unpack_from("<I", cat, 40)[0]
    print("ok   boot catalog at %d, 1.44MB floppy emulation, image at LBA %d" % (cat_lba, img_lba))

    boot = iso[img_lba * SEC:img_lba * SEC + 1474560]
    if boot != floppy:
        fail("boot image in ISO differs from the floppy image")
    if boot[510:512] != b"\x55\xaa":
        fail("boot image has no boot signature")
    print("ok   boot image identical to floppy image (%d bytes)" % len(boot))

    # root directory listing: both-endian fields must agree
    root_lba, root_len = struct.unpack_from("<II", pvd, 156 + 2)
    off, names = root_lba * SEC, []
    while off < root_lba * SEC + root_len and iso[off]:
        ln, nlen = iso[off], iso[off + 32]
        lba_le, lba_be = struct.unpack_from("<I", iso, off + 2)[0], struct.unpack_from(">I", iso, off + 6)[0]
        if lba_le != lba_be:
            fail("both-endian mismatch in directory record")
        names.append(iso[off + 33:off + 33 + nlen])
        off += ln
    print("ok   root directory entries:", [n.decode("latin1") for n in names if len(n) > 1])
    if b"DOS40C.IMG;1" not in names:
        fail("DOS40C.IMG missing from root")
    open("build/iso_boot.img", "wb").write(boot)


if __name__ == "__main__":
    main()
