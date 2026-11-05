#!/usr/bin/env python3
"""run_emu.py IMAGE [--keys TEXT] [--expect STR]...

Boots IMAGE in the minimal emulator, prints what the guest wrote to the
screen, and fails unless every --expect string appears (in both the BIOS
screen stream and the serial stream)."""
import argparse
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
import rmemu


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--keys", default="")
    ap.add_argument("--expect", action="append", default=[])
    ap.add_argument("--max-instr", type=int, default=40_000_000)
    a = ap.parse_args()

    cpu, reason = rmemu.run(a.image, a.keys.encode().decode("unicode_escape").encode("latin1"),
                            a.max_instr)
    text = cpu.screen.decode("latin1")
    print(text.replace("\r\n", "\n"), end="")
    print("--- stopped: %s after %d instructions, exit code %s" %
          (reason, cpu.icount, cpu.exit_code))
    ok = True
    for e in a.expect:
        in_screen = e in text
        in_serial = e in cpu.serial.decode("latin1")
        print("%-4s %s%s" % ("ok" if in_screen and in_serial else "FAIL", e,
                             "" if in_screen == in_serial else "  (screen/serial differ)"))
        ok &= in_screen and in_serial
    if reason.startswith("FAULT"):
        ok = False
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
