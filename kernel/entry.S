/*
 * entry.S - kernel entry point (_kstart)
 *
 * Entered from the boot sector at 1000:0000 with DL = boot drive.
 * All segment registers are set to CS, so the kernel runs in a
 * single 64K "tiny model" segment: DS = ES = SS = CS.
 */
	.code16
	.section .text.start,"ax"
	.global _kstart
	.extern kmain
	.extern __bss_start
	.extern __bss_end

	.equ	KSTACK_TOP, 0xDFF0

_kstart:
	cli
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	movl	$KSTACK_TOP,%esp
	sti
	cld
