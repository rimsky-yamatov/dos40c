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

/* Line editor used by INT 21h/0Ah, by the shell and by stdin reads.
 * Accepts at most max-1 characters; the terminating CR is stored at
 * buf[len] (so buf needs max+1 bytes).  Backspace edits. */
int con_readline(char *buf, int max)
{
	int n = 0;

	for (;;) {
		int c = hal_con_getkey();

		if (c == 0)			/* extended key: ignore */
			continue;
		if (c == '\r') {
			con_puts("\r\n");
			buf[n] = '\r';
			return n;
		}
		if (c == 0x08) {
			if (n > 0) {
				n--;
				con_putc(0x08);
				con_putc(' ');
				con_putc(0x08);
			}
			continue;
		}
		if (c == 0x1B) {		/* ESC: cancel the line */
			while (n > 0) {
				n--;
				con_putc(0x08);
				con_putc(' ');
				con_putc(0x08);
			}
			continue;
		}
		if (c >= 0x20 && n < max - 1) {
			buf[n++] = (char)c;
			con_putc((char)c);
		}
	}
}
