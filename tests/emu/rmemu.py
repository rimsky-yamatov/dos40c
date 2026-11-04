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

    # ---------- string instructions ----------
    def string_op(self, b, osz, seg, rep):
        size = 8 if b in (0xA4, 0xAA, 0xAC) else osz
        step = (size >> 3) * (-1 if self.df else 1)
        sseg = seg if seg is not None else 3
        while True:
            if rep:
                if (self.r[1] & 0xFFFF) == 0:
                    return
            if b in (0xA4, 0xA5):
                v = self.rd(sseg, self.r[6] & 0xFFFF, size)
                self.wr(0, self.r[7] & 0xFFFF, size, v)
                self.r[6] = (self.r[6] & 0xFFFF0000) | ((self.r[6] + step) & 0xFFFF)
                self.r[7] = (self.r[7] & 0xFFFF0000) | ((self.r[7] + step) & 0xFFFF)
            elif b in (0xAA, 0xAB):
                self.wr(0, self.r[7] & 0xFFFF, size, self.rget(0, size))
                self.r[7] = (self.r[7] & 0xFFFF0000) | ((self.r[7] + step) & 0xFFFF)
            else:                                   # lods
                self.rset(0, size, self.rd(sseg, self.r[6] & 0xFFFF, size))
                self.r[6] = (self.r[6] & 0xFFFF0000) | ((self.r[6] + step) & 0xFFFF)
            if not rep:
                return
            self.r[1] = (self.r[1] & 0xFFFF0000) | ((self.r[1] - 1) & 0xFFFF)

    # ---------- port I/O ----------
    def inp(self, port, size):
        if port == 0x3F8:
            return self.loopbyte if self.loop else 0
        if port == 0x3FD:
            return 0x60
        return 0

    def outp(self, port, v, size):
        if port == 0x3FC:
            self.loop = bool(v & 0x10)
        elif port == 0x3F8:
            if self.loop:
                self.loopbyte = v & 0xFF
            else:
                self.serial.append(v & 0xFF)
        elif port == 0xF4:
            self.exit_code = v & 0xFF
            raise Halt("isa-debug-exit")

    # ---------- interrupts ----------
    def do_int(self, n):
        self.push(self.getfl(), 16)
        self.push(self.sr[1], 16)
        self.push(self.ip, 16)
        self.IF = 0
        a = n * 4
        self.ip = int.from_bytes(self.mem[a:a + 2], "little")
        self.sr[1] = int.from_bytes(self.mem[a + 2:a + 4], "little")

    def do_iret(self, size):
        self.ip = self.pop(size) & 0xFFFF
        self.sr[1] = self.pop(size) & 0xFFFF
        fl = self.pop(size)
        self.setfl(fl)

    def bios(self):
        vec = self.ip >> 4
        ah = (self.r[0] >> 8) & 0xFF
        al = self.r[0] & 0xFF
        if vec == 0x10:
            if ah == 0x0E:
                self.screen.append(al)
            elif ah == 0x00:
                self.screen += b"<CLS>"
        elif vec == 0x13:
            if ah == 0x02:
                cl, ch, dh, dl = self.r[1] & 0xFF, (self.r[1] >> 8) & 0xFF, (self.r[2] >> 8) & 0xFF, self.r[2] & 0xFF
                sec, cyl = cl & 0x3F, ch | ((cl & 0xC0) << 2)
                if dl != 0 or sec == 0:
                    self.rset(0, 16, 0x0100)
                    self.cf = 1
                else:
                    lba = (cyl * 2 + dh) * 18 + sec - 1
                    data = self.disk[lba * 512:(lba + al) * 512]
                    a = self.lin(0, self.r[3] & 0xFFFF)
                    self.mem[a:a + len(data)] = data
                    self.rset(0, 16, al)
                    self.cf = 0
            else:
                self.rset(0, 16, 0)
                self.cf = 0
        elif vec == 0x16:
            if ah == 0x00:
                if not self.keys:
                    raise Halt("waiting for keyboard input")
                self.rset(0, 16, self.keys.pop(0))
            elif ah == 0x01:
                if self.keys:
                    self.rset(0, 16, self.keys[0])
                    self.zf = 0
                else:
                    self.zf = 1
        elif vec == 0x19:
            raise Halt("BIOS reboot (INT 19h)")
        else:
            raise Fault("unhandled BIOS vector %#x" % vec)
        # IRET, keeping the CF/ZF results of the service
        cf, zf = self.cf, self.zf
        self.do_iret(16)
        self.cf, self.zf = cf, zf

    # ---------- main decoder ----------
    def step(self):
        if self.sr[1] == 0xF000:
            self.bios()
            return
        self.start_ip = self.ip
        osz = adsz = 16
        seg = None
        rep = 0
        while True:
            b = self.f8()
            if b == 0x66:
                osz = 32
            elif b == 0x67:
                adsz = 32
            elif b in SEGPFX:
                seg = SEGPFX[b]
            elif b in (0xF2, 0xF3):
                rep = b
            else:
                break
        r = self.r

        if b < 0x40 and (b & 7) < 6:                      # 00-3F ALU
            op, k = b >> 3, b & 7
            size = 8 if k in (0, 2, 4) else osz
            if k < 4:
                self.modrm(adsz, seg)
                if k < 2:
                    res = self.alu(op, self.rm_get(size), self.rget(self.reg, size), size)
                    if op != 7:
                        self.rm_set(size, res)
                else:
                    res = self.alu(op, self.rget(self.reg, size), self.rm_get(size), size)
                    if op != 7:
                        self.rset(self.reg, size, res)
            else:
                res = self.alu(op, self.rget(0, size), self.fimm(size), size)
                if op != 7:
                    self.rset(0, size, res)
        elif b in (0x06, 0x0E, 0x16, 0x1E):
            self.push(self.sr[{0x06: 0, 0x0E: 1, 0x16: 2, 0x1E: 3}[b]], osz)
        elif b in (0x07, 0x17, 0x1F):
            self.sr[{0x07: 0, 0x17: 2, 0x1F: 3}[b]] = self.pop(osz) & 0xFFFF
        elif 0x40 <= b <= 0x4F:                           # inc/dec r
            n, cf = b & 7, self.cf
            self.rset(n, osz, self.alu(0 if b < 0x48 else 5, self.rget(n, osz), 1, osz))
            self.cf = cf
        elif 0x50 <= b <= 0x57:
            self.push(self.rget(b & 7, osz), osz)
        elif 0x58 <= b <= 0x5F:
            self.rset(b & 7, osz, self.pop(osz))
        elif b == 0x60:
            sp0 = self.rget(4, osz)
            for n in (0, 1, 2, 3):
                self.push(self.rget(n, osz), osz)
            self.push(sp0, osz)
            for n in (5, 6, 7):
                self.push(self.rget(n, osz), osz)
        elif b == 0x61:
            for n in (7, 6, 5):
                self.rset(n, osz, self.pop(osz))
            self.pop(osz)
            for n in (3, 2, 1, 0):
                self.rset(n, osz, self.pop(osz))
        elif b == 0x68:
            self.push(self.fimm(osz), osz)
        elif b == 0x6A:
            self.push(self.f8s() & ((1 << osz) - 1), osz)
        elif b in (0x69, 0x6B):
            self.modrm(adsz, seg)
            imm = self.fimm(osz) if b == 0x69 else self.f8s()
            v = sx(self.rm_get(osz), osz) * sx(imm & ((1 << osz) - 1), osz)
            self.rset(self.reg, osz, v)
            self.cf = self.of = int(sx(v & ((1 << osz) - 1), osz) != v)
        elif 0x70 <= b <= 0x7F:
            d = self.f8s()
            if self.cond(b & 15):
                self.ip = (self.ip + d) & 0xFFFF
        elif b in (0x80, 0x81, 0x82, 0x83):
            size = 8 if b in (0x80, 0x82) else osz
            self.modrm(adsz, seg)
            imm = self.f8() if b in (0x80, 0x82) else self.fimm(osz) if b == 0x81 else self.f8s() & ((1 << osz) - 1)
            res = self.alu(self.reg, self.rm_get(size), imm, size)
            if self.reg != 7:
                self.rm_set(size, res)
        elif b in (0x84, 0x85):
            size = 8 if b == 0x84 else osz
            self.modrm(adsz, seg)
            self.alu(4, self.rm_get(size), self.rget(self.reg, size), size)
        elif b in (0x86, 0x87):
            size = 8 if b == 0x86 else osz
            self.modrm(adsz, seg)
            a, c = self.rm_get(size), self.rget(self.reg, size)
            self.rm_set(size, c)
            self.rset(self.reg, size, a)
        elif b in (0x88, 0x89, 0x8A, 0x8B):
            size = 8 if b in (0x88, 0x8A) else osz
            self.modrm(adsz, seg)
            if b < 0x8A:
                self.rm_set(size, self.rget(self.reg, size))
            else:
                self.rset(self.reg, size, self.rm_get(size))
        elif b == 0x8C:
            self.modrm(adsz, seg)
            self.rm_set(16 if self.mod != 3 else osz, self.sr[self.reg])
        elif b == 0x8D:
            self.modrm(adsz, seg)
            self.rset(self.reg, osz, self.eoff & ((1 << osz) - 1))
        elif b == 0x8E:
            self.modrm(adsz, seg)
            self.sr[self.reg] = self.rm_get(16) & 0xFFFF
        elif b == 0x8F:
            self.modrm(adsz, seg)
            self.rm_set(osz, self.pop(osz))
        elif b == 0x90:
            pass
        elif b == 0x98:
            if osz == 16:
                self.rset(0, 16, sx(self.rget(0, 8), 8))
            else:
                self.rset(0, 32, sx(self.rget(0, 16), 16))
        elif b == 0x99:
            self.rset(2, osz, ((1 << osz) - 1) if self.rget(0, osz) >> (osz - 1) else 0)
        elif b == 0x9C:
            self.push(self.getfl(), osz)
        elif b == 0x9D:
            self.setfl(self.pop(osz))
        elif b in (0xA0, 0xA1, 0xA2, 0xA3):
            size = 8 if b in (0xA0, 0xA2) else osz
            off = self.f16() if adsz == 16 else self.f32()
            s_ = seg if seg is not None else 3
            if b < 0xA2:
                self.rset(0, size, self.rd(s_, off, size))
            else:
                self.wr(s_, off, size, self.rget(0, size))
        elif b in (0xA4, 0xA5, 0xAA, 0xAB, 0xAC, 0xAD):
            self.string_op(b, osz, seg, rep)
        elif b in (0xA8, 0xA9):
            size = 8 if b == 0xA8 else osz
            self.alu(4, self.rget(0, size), self.fimm(size), size)
        elif 0xB0 <= b <= 0xB7:
            self.rset(b & 7, 8, self.f8())
        elif 0xB8 <= b <= 0xBF:
            self.rset(b & 7, osz, self.fimm(osz))
        elif b in (0xC0, 0xC1, 0xD0, 0xD1, 0xD2, 0xD3):
            size = 8 if b in (0xC0, 0xD0, 0xD2) else osz
            self.modrm(adsz, seg)
            cnt = self.f8() if b in (0xC0, 0xC1) else 1 if b in (0xD0, 0xD1) else self.rget(1, 8)
            self.rm_set(size, self.shift(self.reg, self.rm_get(size), cnt, size))
        elif b in (0xC2, 0xC3):
            n = self.f16() if b == 0xC2 else 0
            self.ip = self.pop(osz) & 0xFFFF
            self.r[4] = (self.r[4] & 0xFFFF0000) | ((self.r[4] + n) & 0xFFFF)
        elif b in (0xC6, 0xC7):
            size = 8 if b == 0xC6 else osz
            self.modrm(adsz, seg)
            self.rm_set(size, self.fimm(size))
        elif b == 0xC9:
            self.r[4] = (self.r[4] & 0xFFFF0000) | (self.r[5] & 0xFFFF)
            self.rset(5, osz, self.pop(osz))
        elif b == 0xCD:
            self.do_int(self.f8())
        elif b == 0xCF:
            self.do_iret(osz)
        elif b in (0xE4, 0xE5, 0xEC, 0xED):
            size = 8 if b in (0xE4, 0xEC) else osz
            port = self.f8() if b < 0xE8 else self.r[2] & 0xFFFF
            self.rset(0, size, self.inp(port, size))
        elif b in (0xE6, 0xE7, 0xEE, 0xEF):
            size = 8 if b in (0xE6, 0xEE) else osz
            port = self.f8() if b < 0xE8 else self.r[2] & 0xFFFF
            self.outp(port, self.rget(0, size), size)
        elif b == 0xE8:
            d = self.f16s() if osz == 16 else self.f32s()
            self.push(self.ip, osz)
            self.ip = (self.ip + d) & M32
            if self.ip > 0xFFFF:
                raise Fault("call target %#x" % self.ip)
        elif b in (0xE9, 0xEB):
            d = self.f8s() if b == 0xEB else self.f16s() if osz == 16 else self.f32s()
            self.ip = (self.ip + d) & M32
            if self.ip > 0xFFFF:
                raise Fault("jump target %#x" % self.ip)
        elif b == 0xEA:
            off = self.fimm(osz)
            self.sr[1] = self.f16()
            self.ip = off & 0xFFFF
        elif b == 0xF4:
            raise Halt("hlt")
        elif b in (0xF6, 0xF7):
            self.group3(b, osz, adsz, seg)
        elif b == 0xFA:
            self.IF = 0
        elif b == 0xFB:
            self.IF = 1
        elif b == 0xFC:
            self.df = 0
        elif b == 0xFD:
            self.df = 1
        elif b in (0xFE, 0xFF):
            self.group45(b, osz, adsz, seg)
        elif b == 0x0F:
            self.twobyte(osz, adsz, seg)
        else:
            raise Fault("unsupported opcode %02x at %04x:%04x" % (b, self.sr[1], self.start_ip))
