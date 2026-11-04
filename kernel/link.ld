/* link.ld - kernel image, loaded at 1000:0000, offsets start at 0 */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_kstart)
SECTIONS
{
	. = 0;
	.text   : { *(.text.start) *(.text .text.*) }
	.rodata : { *(.rodata .rodata.*) }
	.data   : { *(.data .data.*) }
	__bss_start = .;
	.bss    : { *(.bss .bss.*) *(COMMON) }
	__bss_end = .;
	/DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) }
}
/* 0xC800.. is left for the kernel stack (grows down from 0xDFF0)
   and 0xE000..0xFFF0 for the INT 21h stack */
ASSERT(__bss_end <= 0xC800, "kernel image too large for 64K segment layout")
