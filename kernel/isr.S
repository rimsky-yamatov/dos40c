/*
 * isr.S - INT 20h / INT 21h entry stubs and program launch/return
 *
 * Programs run on their own stack in their own segment.  The stubs
 * save the caller's registers, switch to the kernel segment and
 * kernel interrupt stack, and hand a copy of the register frame
 * (struct regs / g_regs) to the C dispatcher dos_int21().
 */
	.code16
	.text
	.global int20_entry, int21_entry
	.global run_com, exit_to_kernel
	.extern g_regs
	.extern dos_int21

	.equ	ISR_TOP, 0xFFF0
	.equ	FRAME_WORDS, 13		/* di si bp sp bx dx cx ax ds es ip cs fl */

/* INT 20h: terminate program == INT 21h function 00h */
int20_entry:
	xorw	%ax,%ax
	/* fall through */

int21_entry:
	pushw	%es
	pushw	%ds
	pushaw
	movw	%ss,%cs:u_ss		/* remember caller's stack */
	movw	%sp,%cs:u_sp
	cld
	/* copy the 13-word frame from the caller's stack into g_regs */
	movw	%ss,%ax
	movw	%ax,%ds
	movw	%sp,%si
	movw	%cs,%ax
	movw	%ax,%es
	movw	$g_regs,%di
	movw	$FRAME_WORDS,%cx
	rep	movsw
	/* switch to the kernel segment and interrupt stack */
	movw	%ax,%ds
	movw	%ax,%ss
	movl	$ISR_TOP,%esp
	sti
	calll	dos_int21

	cli
	/* copy the (possibly modified) frame back to the caller's stack */
	movw	%cs:u_ss,%ax
	movw	%ax,%es
	movw	%cs:u_sp,%di
	movw	$g_regs,%si
	movw	$FRAME_WORDS,%cx
	cld
	rep	movsw
	movw	%cs:u_ss,%ss
	movw	%cs:u_sp,%sp
	popaw
	popw	%ds
	popw	%es
	iret
