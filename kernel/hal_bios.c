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
