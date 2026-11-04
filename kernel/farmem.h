/* farmem.h - access to memory outside the kernel segment (user programs,
 * the interrupt vector table).  The kernel runs with DS = ES = SS. */
#ifndef DOS40C_FARMEM_H
#define DOS40C_FARMEM_H

#include "types.h"

static inline u16 kseg(void)
{
	u16 s;

	asm volatile("movw %%ds,%0" : "=r"(s));
	return s;
}

static inline u8 far_rd8(u16 seg, u16 off)
{
	u8 v;

	asm volatile("pushw %%fs; movw %w1,%%fs; movb %%fs:(%2),%0; popw %%fs"
		     : "=&q"(v)
		     : "r"((u32)seg), "r"((u32)off)
		     : "memory");
	return v;
}

static inline void far_wr8(u16 seg, u16 off, u8 v)
{
	asm volatile("pushw %%fs; movw %w0,%%fs; movb %2,%%fs:(%1); popw %%fs"
		     :
		     : "r"((u32)seg), "r"((u32)off), "q"(v)
		     : "memory");
}

static inline void far_wr16(u16 seg, u16 off, u16 v)
{
	asm volatile("pushw %%fs; movw %w0,%%fs; movw %w2,%%fs:(%1); popw %%fs"
		     :
		     : "r"((u32)seg), "r"((u32)off), "r"((u32)v)
		     : "memory");
}

static inline u16 far_rd16(u16 seg, u16 off)
{
	u16 v;

	asm volatile("pushw %%fs; movw %w1,%%fs; movw %%fs:(%2),%w0; popw %%fs"
		     : "=&r"(v)
		     : "r"((u32)seg), "r"((u32)off)
		     : "memory");
	return v;
}
