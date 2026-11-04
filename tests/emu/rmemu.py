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

    # ---------- memory / registers ----------
    def lin(self, seg, off):
        if off > 0xFFFF:
            raise Fault("offset %#x outside segment at %04x:%04x" %
                        (off, self.sr[1], self.start_ip))
        return ((self.sr[seg] << 4) + off) & 0xFFFFF

    def rd(self, seg, off, size):
        a = self.lin(seg, off)
        return int.from_bytes(self.mem[a:a + (size >> 3)], "little")

    def wr(self, seg, off, size, v):
        a = self.lin(seg, off)
        self.mem[a:a + (size >> 3)] = (v & ((1 << size) - 1)).to_bytes(size >> 3, "little")

    def f8(self):
        b = self.mem[((self.sr[1] << 4) + self.ip) & 0xFFFFF]
        self.ip = (self.ip + 1) & 0xFFFF
        return b

    def f16(self):
        return self.f8() | (self.f8() << 8)

    def f32(self):
        return self.f16() | (self.f16() << 16)

    def f8s(self):
        return sx(self.f8(), 8)

    def f16s(self):
        return sx(self.f16(), 16)

    def f32s(self):
        return sx(self.f32(), 32)

    def fimm(self, size):
        return self.f8() if size == 8 else self.f16() if size == 16 else self.f32()

    def rget(self, n, size):
        if size == 32:
            return self.r[n]
        if size == 16:
            return self.r[n] & 0xFFFF
        return (self.r[n & 3] >> (8 if n >= 4 else 0)) & 0xFF

    def rset(self, n, size, v):
        if size == 32:
            self.r[n] = v & M32
        elif size == 16:
            self.r[n] = (self.r[n] & 0xFFFF0000) | (v & 0xFFFF)
        else:
            i = n & 3
            if n >= 4:
                self.r[i] = (self.r[i] & 0xFFFF00FF) | ((v & 0xFF) << 8)
            else:
                self.r[i] = (self.r[i] & 0xFFFFFF00) | (v & 0xFF)

    def push(self, v, size):
        sp = (self.r[4] - (size >> 3)) & 0xFFFF
        self.r[4] = (self.r[4] & 0xFFFF0000) | sp
        self.wr(2, sp, size, v)

    def pop(self, size):
        sp = self.r[4] & 0xFFFF
        v = self.rd(2, sp, size)
        self.r[4] = (self.r[4] & 0xFFFF0000) | ((sp + (size >> 3)) & 0xFFFF)
        return v

    def getfl(self):
        return (self.cf | 2 | self.pf << 2 | self.af << 4 | self.zf << 6 |
                self.sf << 7 | self.IF << 9 | self.df << 10 | self.of << 11)

    def setfl(self, v):
        self.cf = v & 1
        self.pf = (v >> 2) & 1
        self.af = (v >> 4) & 1
        self.zf = (v >> 6) & 1
        self.sf = (v >> 7) & 1
        self.IF = (v >> 9) & 1
        self.df = (v >> 10) & 1
        self.of = (v >> 11) & 1

    # ---------- ModRM decoding ----------
    def modrm(self, adsz, seg):
        m = self.f8()
        self.mod, self.reg, self.rm = m >> 6, (m >> 3) & 7, m & 7
        mod, rm, r = self.mod, self.rm, self.r
        if mod == 3:
            return
        if adsz == 16:
            bx, bp, si, di = r[3] & 0xFFFF, r[5] & 0xFFFF, r[6] & 0xFFFF, r[7] & 0xFFFF
            if mod == 0 and rm == 6:
                off, dseg = self.f16(), 3
            else:
                base = (bx + si, bx + di, bp + si, bp + di, si, di, bp, bx)[rm]
                dseg = 2 if rm in (2, 3, 6) else 3
                disp = self.f8s() if mod == 1 else self.f16s() if mod == 2 else 0
                off = (base + disp) & 0xFFFF
        else:
            dseg = 3
            if rm == 4:
                sib = self.f8()
                sc, ix, bs = sib >> 6, (sib >> 3) & 7, sib & 7
                index = 0 if ix == 4 else (r[ix] << sc)
                nobase = bs == 5 and mod == 0
                base = 0 if nobase else r[bs]
                if not nobase and bs in (4, 5):
                    dseg = 2
            elif rm == 5 and mod == 0:
                index, base, nobase = 0, 0, True
            else:
                index, base, nobase = 0, r[rm], False
                if rm == 5:
                    dseg = 2
            disp = self.f8s() if mod == 1 else self.f32s() if (mod == 2 or nobase) else 0
            off = (base + index + disp) & M32
        self.eseg = seg if seg is not None else dseg
        self.eoff = off

    def rm_get(self, size):
        if self.mod == 3:
            return self.rget(self.rm, size)
        return self.rd(self.eseg, self.eoff, size)

    def rm_set(self, size, v):
        if self.mod == 3:
            self.rset(self.rm, size, v)
        else:
            self.wr(self.eseg, self.eoff, size, v)

    # ---------- ALU ----------
    def szp(self, r, size):
        r &= (1 << size) - 1
        self.zf = int(r == 0)
        self.sf = (r >> (size - 1)) & 1
        self.pf = int(PARITY[r & 0xFF])

    def alu(self, op, a, b, size):
        mask = (1 << size) - 1
        sign = 1 << (size - 1)
        if op in (0, 2):
            c = self.cf if op == 2 else 0
            r = a + b + c
            self.cf = int(r > mask)
            self.of = int(bool(~(a ^ b) & (a ^ r) & sign))
            self.af = int(bool((a ^ b ^ r) & 16))
        elif op in (3, 5, 7):
            c = self.cf if op == 3 else 0
            r = a - b - c
            self.cf = int(a < b + c)
            self.of = int(bool((a ^ b) & (a ^ r) & sign))
            self.af = int(bool((a ^ b ^ r) & 16))
        else:
            r = a | b if op == 1 else a & b if op == 4 else a ^ b
            self.cf = self.of = 0
        r &= mask
        self.szp(r, size)
        return r

    def cond(self, n):
        c = n >> 1
        v = (self.of, self.cf, self.zf, self.cf or self.zf, self.sf, self.pf,
             self.sf != self.of, self.zf or self.sf != self.of)[c]
        return bool(v) ^ bool(n & 1)

    def shift(self, op, v, cnt, size):
        mask = (1 << size) - 1
        cnt &= 31
        if cnt == 0:
            return v
        if op in (4, 6):
            r = (v << cnt) & mask
            self.cf = (v >> (size - cnt)) & 1 if cnt <= size else 0
            self.of = ((r >> (size - 1)) & 1) ^ self.cf
        elif op == 5:
            r = v >> cnt
            self.cf = (v >> (cnt - 1)) & 1
            self.of = (v >> (size - 1)) & 1
        elif op == 7:
            sv = sx(v, size)
            r = (sv >> cnt) & mask
            self.cf = (sv >> (cnt - 1)) & 1
            self.of = 0
        elif op == 0:
            cnt %= size
            r = ((v << cnt) | (v >> (size - cnt))) & mask
            self.cf = r & 1
        elif op == 1:
            cnt %= size
            r = ((v >> cnt) | (v << (size - cnt))) & mask
            self.cf = (r >> (size - 1)) & 1
        else:
            raise Fault("unsupported shift /%d" % op)
        if op >= 4:
            self.szp(r, size)
        return r
