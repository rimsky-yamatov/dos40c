/* hal_bios.c - BIOS-backed HAL (replaces MSBIO/MSCON/MSDISK of the original) */
#include "hal.h"

static inline void outb(u16 port, u8 v)
{
	asm volatile("outb %0,%w1" : : "a"(v), "Nd"(port));
}

static inline u8 inb(u16 port)
{
	u8 v;
	asm volatile("inb %w1,%0" : "=a"(v) : "Nd"(port));
	return v;
}

static inline void outw(u16 port, u16 v)
{
	asm volatile("outw %0,%w1" : : "a"(v), "Nd"(port));
}

/* ---------------- serial (COM1, 115200 8N1) ---------------- */
#define COM1 0x3F8

static u8 ser_present;

static void ser_init(void)
{
	/* loopback test to detect a real UART */
	outb(COM1 + 4, 0x1E);
	outb(COM1 + 0, 0xAE);
	if (inb(COM1 + 0) != 0xAE) {
		ser_present = 0;
		return;
	}
	outb(COM1 + 1, 0x00);	/* no interrupts */
	outb(COM1 + 3, 0x80);	/* DLAB on */
	outb(COM1 + 0, 0x01);	/* divisor 1 = 115200 */
	outb(COM1 + 1, 0x00);
	outb(COM1 + 3, 0x03);	/* 8N1 */
	outb(COM1 + 2, 0xC7);	/* FIFO on */
	outb(COM1 + 4, 0x0B);	/* DTR RTS OUT2 */
	ser_present = 1;
}

static void ser_putc(char c)
{
	u16 t = 0xFFFF;

	if (!ser_present)
		return;
	while (!(inb(COM1 + 5) & 0x20) && --t)
		;
	outb(COM1, (u8)c);
}

static int ser_ready(void)
{
	return ser_present && (inb(COM1 + 5) & 0x01);
}

/* ---------------- console ---------------- */
void hal_con_putc(char c)
{
	u32 ax = 0x0E00 | (u8)c;
	u32 bx = 0x0007;

	asm volatile("int $0x10"
		     : "+a"(ax), "+b"(bx)
		     :
		     : "ecx", "edx", "esi", "edi", "cc", "memory");
	ser_putc(c);
}

void hal_con_init(void)
{
	ser_init();
}

void hal_con_cls(void)
{
	u32 ax = 0x0003;	/* set text mode 3 = clear screen */

	asm volatile("int $0x10"
		     : "+a"(ax)
		     :
		     : "ebx", "ecx", "edx", "esi", "edi", "cc", "memory");
	ser_putc(0x1B);
	ser_putc('[');
	ser_putc('2');
	ser_putc('J');
	ser_putc(0x1B);
	ser_putc('[');
	ser_putc('H');
}

static int bios_kbhit(void)
{
	u32 ax = 0x0100;
	int avail;

	asm volatile("int $0x16"
		     : "=@ccnz"(avail), "+a"(ax)
		     :
		     : "ebx", "ecx", "edx", "esi", "edi", "memory");
	return avail;
}

static int bios_getkey(void)
{
	u32 ax = 0x0000;

	asm volatile("int $0x16"
		     : "+a"(ax)
		     :
		     : "ebx", "ecx", "edx", "esi", "edi", "cc", "memory");
	return (int)(ax & 0xFF);
}

int hal_con_kbhit(void)
{
	return bios_kbhit() || ser_ready();
}

int hal_con_getkey(void)
{
	for (;;) {
		if (bios_kbhit())
			return bios_getkey();
		if (ser_ready()) {
			int c = inb(COM1);

			if (c == '\n')	/* terminals send LF for Enter */
				c = '\r';
			if (c == 0x7F)	/* DEL -> BS */
				c = 0x08;
			return c;
		}
	}
}

/* ---------------- disk ---------------- */
static u16 geo_spt = 18, geo_heads = 2;

void hal_set_geometry(u8 drive, u16 spt, u16 heads)
{
	(void)drive;
	if (spt && heads) {
		geo_spt = spt;
		geo_heads = heads;
	}
}

int hal_read_sector(u8 drive, u32 lba, void *buf)
{
	u32 track = lba / geo_spt;
	u32 sec   = lba % geo_spt + 1;
	u32 head  = track % geo_heads;
	u32 cyl   = track / geo_heads;
	int tries;

	for (tries = 0; tries < 4; tries++) {
		u32 ax = 0x0201;
		u32 cx = ((cyl & 0xFF) << 8) | ((cyl >> 2) & 0xC0) | sec;
		u32 dx = (head << 8) | drive;
		int cf;

		asm volatile("int $0x13"
			     : "=@ccc"(cf), "+a"(ax), "+c"(cx), "+d"(dx)
			     : "b"((u32)(uintptr_t)buf)
			     : "esi", "edi", "memory");
		if (!cf)
			return 0;
		ax = 0;		/* reset controller and retry */
		dx = drive;
		asm volatile("int $0x13"
			     : "+a"(ax), "+d"(dx)
			     :
			     : "ebx", "ecx", "esi", "edi", "cc", "memory");
	}
	return -1;
}

/* ---------------- machine control ---------------- */
void hal_halt(int code)
{
	outb(0xF4, (u8)code);		/* QEMU isa-debug-exit (if present) */
	outw(0x604, 0x2000);		/* QEMU ACPI power off (if present) */
	for (;;)
		asm volatile("cli; hlt");
}
