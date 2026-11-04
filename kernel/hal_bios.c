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
