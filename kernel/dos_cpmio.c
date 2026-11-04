/* dos_cpmio.c - console I/O, ported from DOS/STDCPMIO.ASM and STRIN.ASM */
#include "dos.h"
#include "hal.h"

void con_putc(char c)
{
	hal_con_putc(c);
}

void con_puts(const char *s)
{
	while (*s)
		con_putc(*s++);
}

int con_getch(void)
{
	return hal_con_getkey();
}

void con_flush_input(void)
{
	while (hal_con_kbhit())
		(void)hal_con_getkey();
}
