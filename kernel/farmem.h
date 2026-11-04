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

/* kernel buffer -> seg:off */
static inline void far_copy_to(u16 seg, u16 off, const void *src, u16 n)
{
	u32 d = off, s = (u32)(uintptr_t)src, c = n;

	asm volatile("pushw %%es; movw %w3,%%es; cld; rep movsb; popw %%es"
		     : "+D"(d), "+S"(s), "+c"(c)
		     : "r"((u32)seg)
		     : "memory");
}

/* seg:off -> kernel buffer */
static inline void far_copy_from(void *dst, u16 seg, u16 off, u16 n)
{
	u32 d = (u32)(uintptr_t)dst, s = off, c = n;

	asm volatile("pushw %%ds; movw %w3,%%ds; cld; rep movsb; popw %%ds"
		     : "+D"(d), "+S"(s), "+c"(c)
		     : "r"((u32)seg)
		     : "memory");
}

/* ASCIIZ string at seg:off -> kernel buffer (truncating) */
static inline void far_get_str(char *dst, u16 seg, u16 off, u16 max)
{
	u16 i;

	for (i = 0; i + 1 < max; i++) {
		char c = (char)far_rd8(seg, (u16)(off + i));

		dst[i] = c;
		if (!c)
			return;
	}
	dst[i] = 0;
}

#endif
