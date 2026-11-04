/* hello.S - sample .COM program: prints a greeting through INT 21h/09h,
 * queries the DOS version (INT 21h/30h) and exits with code 0. */
	.code16
	.text
	.global _start
_start:
	movw	$msg,%dx
	movb	$0x09,%ah
	int	$0x21

	movb	$0x30,%ah		/* get DOS version */
	int	$0x21
	addb	$'0',%al		/* AL = major version */
	movb	%al,ver
	movw	$vermsg,%dx
	movb	$0x09,%ah
	int	$0x21

	movw	$0x4C00,%ax		/* exit(0) */
	int	$0x21

msg:	.ascii	"Hello from a DOS COM program!\r\n$"
vermsg:	.ascii	"INT 21h/30h reports DOS major version "
ver:	.ascii	"?\r\n$"
