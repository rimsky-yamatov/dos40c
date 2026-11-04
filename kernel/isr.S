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
