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

/* ---- BIOS Parameter Block (offsets match FAT12 spec) ---- */
	.ascii	"DOS40C  "		/* 03: OEM name              */
	.word	512			/* 0B: bytes per sector      */
	.byte	1			/* 0D: sectors per cluster   */
bpb_reserved:
	.word	1			/* 0E: reserved (patched)    */
	.byte	2			/* 10: number of FATs        */
	.word	224			/* 11: root directory entries*/
	.word	2880			/* 13: total sectors         */
	.byte	0xF0			/* 15: media descriptor      */
	.word	9			/* 16: sectors per FAT       */
bpb_spt:
	.word	18			/* 18: sectors per track     */
bpb_heads:
	.word	2			/* 1A: heads                 */
	.long	0			/* 1C: hidden sectors        */
	.long	0			/* 20: total sectors (32bit) */
bs_drive:
	.byte	0			/* 24: BIOS drive number     */
	.byte	0			/* 25: reserved              */
	.byte	0x29			/* 26: extended boot sig     */
	.long	0x40C0DE00		/* 27: volume serial         */
	.ascii	"DOS40C     "		/* 2B: volume label          */
	.ascii	"FAT12   "		/* 36: file system type      */

boot:
	cli
	xorw	%ax,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	movw	$0x7C00,%sp
	sti
	cld
	movb	%dl,bs_drive		/* remember boot drive */

	movw	bpb_reserved,%ax
	decw	%ax			/* kernel occupies reserved-1 sectors */
	movw	%ax,left
	movw	$1,cur_lba
	movw	$0x1000,%ax		/* load address 1000:0000 */
	movw	%ax,%es
	xorw	%bx,%bx

next_sector:
	cmpw	$0,left
	je	done
	/* LBA -> CHS */
	movw	cur_lba,%ax
	xorw	%dx,%dx
	divw	bpb_spt			/* AX = track, DX = sector-1 */
	movb	%dl,%cl
	incb	%cl			/* CL = sector (1-based) */
	xorw	%dx,%dx
	divw	bpb_heads		/* AX = cylinder, DX = head */
	movb	%al,%ch			/* CH = cylinder (floppy: < 256) */
	movb	%dl,%dh			/* DH = head */
	movb	bs_drive,%dl		/* DL = drive */
	movw	$4,retry
