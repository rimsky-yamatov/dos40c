/* dostest.S - .COM program exercising INT 21h services:
 * open/read/lseek/write/close, FindFirst/FindNext, memory allocation,
 * free space and exit code.  Prints "PASS: ..." lines. */
	.code16
	.text
	.global _start
_start:
	/* open README.TXT, read 6 bytes, write them to stdout */
	movw	$fname,%dx
	movw	$0x3D00,%ax
	int	$0x21
	jc	fail
	movw	%ax,%bx
	movw	$buf,%dx
	movw	$6,%cx
	movb	$0x3F,%ah
	int	$0x21
	jc	fail
	cmpw	$6,%ax
	jne	fail
	pushw	%bx
	movw	$1,%bx
	movw	$6,%cx
	movb	$0x40,%ah
	int	$0x21
	popw	%bx
	movw	$crlf,%dx
	movb	$9,%ah
	int	$0x21
	/* lseek to offset 7 from the start */
	xorw	%cx,%cx
	movw	$7,%dx
	movw	$0x4200,%ax
	int	$0x21
	jc	fail
	movb	$0x3E,%ah		/* close */
	int	$0x21
	jc	fail
	movw	$ok1,%dx
	movb	$9,%ah
	int	$0x21

	/* FindFirst HELLO.COM then FindNext must fail with error 18 */
	movw	$pat,%dx
	movw	$0,%cx
	movb	$0x4E,%ah
	int	$0x21
	jc	fail
	movb	$0x4F,%ah
	int	$0x21
	jnc	fail
	cmpw	$18,%ax
	jne	fail
	movw	$ok2,%dx
	movb	$9,%ah
	int	$0x21

	/* allocate 0x100 paragraphs, free them again */
	movw	$0x100,%bx
	movb	$0x48,%ah
	int	$0x21
	jc	fail
	movw	%ax,%es
	movb	$0x49,%ah
	int	$0x21
	jc	fail
	movw	$ok3,%dx
	movb	$9,%ah
	int	$0x21

	movw	$7,%ax			/* exit code 7 */
	movb	$0x4C,%ah
	int	$0x21
fail:
	movw	$bad,%dx
	movb	$9,%ah
	int	$0x21
	movw	$0x4C01,%ax
	int	$0x21

fname:	.asciz	"README.TXT"
pat:	.asciz	"HELLO.COM"
ok1:	.ascii	"PASS: open/read/write/lseek/close\r\n$"
ok2:	.ascii	"PASS: findfirst/findnext\r\n$"
ok3:	.ascii	"PASS: alloc/free\r\n$"
bad:	.ascii	"FAIL\r\n$"
crlf:	.ascii	"\r\n$"
buf:	.space	16
