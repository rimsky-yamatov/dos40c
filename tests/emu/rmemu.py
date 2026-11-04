#!/usr/bin/env python3
"""rmemu.py - minimal 386 real-mode emulator used to smoke-test DOS40C
without QEMU.  It implements exactly the instruction subset that the
boot sector and the GCC -m16 kernel use, plus tiny BIOS (INT 10h/13h/16h)
and serial-port models.  Unknown opcodes raise an error, and addressing
outside a 64K segment faults like real hardware would.
"""
M32 = 0xFFFFFFFF
SEGPFX = {0x26: 0, 0x2E: 1, 0x36: 2, 0x3E: 3, 0x64: 4, 0x65: 5}
PARITY = [bin(i).count("1") % 2 == 0 for i in range(256)]


class Halt(Exception):
    pass


class Fault(Exception):
    pass


def sx(v, size):
    return v - (1 << size) if v & (1 << (size - 1)) else v


class CPU:
    def __init__(self, disk, keys=b""):
        self.mem = bytearray(0x110000)
        self.r = [0] * 8            # eax ecx edx ebx esp ebp esi edi
        self.sr = [0] * 6           # es cs ss ds fs gs
        self.ip = 0
        self.cf = self.zf = self.sf = self.of = self.pf = self.af = self.df = 0
        self.IF = 1
        self.disk = disk
        self.keys = bytearray(keys)
        self.serial = bytearray()
        self.screen = bytearray()
        self.loop = False
        self.loopbyte = 0
        self.exit_code = None
        self.icount = 0
        self.trace = []
        for v in (0x10, 0x13, 0x16, 0x19):      # BIOS entry points
            self.mem[v * 4:v * 4 + 4] = (v * 16).to_bytes(2, "little") + b"\x00\xf0"
