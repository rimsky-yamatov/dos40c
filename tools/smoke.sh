#!/bin/sh
# smoke.sh IMAGE - boot the self-test image in QEMU and check the serial log
set -u
IMG=${1:?usage: smoke.sh IMAGE}
QEMU=${QEMU:-qemu-system-i386}
LOG=${LOG:-build/serial.log}
mkdir -p "$(dirname "$LOG")"
rm -f "$LOG"

timeout 60 "$QEMU" -display none -no-reboot -boot a \
	-drive file="$IMG",format=raw,if=floppy \
	-serial file:"$LOG" \
	-device isa-debug-exit,iobase=0xf4,iosize=0x04
rc=$?
echo "qemu exit status: $rc (33 = guest powered off via HALT)"
echo "----- serial log -----"
cat "$LOG"
echo "----------------------"

fail=0
for s in "SELFTEST BEGIN" "Volume in drive A is DOS40C" "HELLO    COM" \
         "Hello from a DOS COM program!" "reports DOS major version 4" \
         "SELFTEST END" "DOS40C: HALT"; do
	if grep -q "$s" "$LOG"; then echo "ok   $s"; else echo "FAIL $s"; fail=1; fi
done
[ "$rc" -eq 33 ] || { echo "FAIL unexpected qemu exit status"; fail=1; }
exit $fail
