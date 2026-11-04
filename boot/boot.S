/*
 * boot.S - FAT12 boot sector for DOS40C
 *
 * Replaces BOOT/MSBOOT.ASM of MS-DOS 4.0.  Instead of searching the
 * root directory for IO.SYS, the kernel image lives in the reserved
 * sectors that follow this sector (sector 1 .. reserved-1).  The
 * `reserved` BPB field is patched by tools/mkfat.
 *
 * The kernel is loaded to 1000:0000 and entered with DL = boot drive.
 */
	.code16
	.text
	.global _start
_start:
	jmp	boot
	nop
