/* lib.c - memory / string primitives */
#include "lib.h"

void *memcpy(void *dst, const void *src, size_t n)
{
	void *d = dst;

	asm volatile("cld; rep movsb"
		     : "+D"(dst), "+S"(src), "+c"(n)
		     :
		     : "memory");
	return d;
}

void *memmove(void *dst, const void *src, size_t n)
{
	u8 *d = dst;
	const u8 *s = src;

	if (d == s || n == 0)
		return dst;
	if (d < s)
		return memcpy(dst, src, n);
	d += n - 1;
	s += n - 1;
	asm volatile("std; rep movsb; cld"
		     : "+D"(d), "+S"(s), "+c"(n)
		     :
		     : "memory");
	return dst;
}

void *memset(void *dst, int c, size_t n)
{
	void *d = dst;

	asm volatile("cld; rep stosb"
		     : "+D"(dst), "+c"(n)
		     : "a"(c)
		     : "memory");
	return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
	const u8 *p = a, *q = b;

	while (n--) {
		if (*p != *q)
			return *p - *q;
		p++;
		q++;
	}
	return 0;
}

size_t strlen(const char *s)
{
	size_t n = 0;

	while (s[n])
		n++;
	return n;
}

int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return (u8)*a - (u8)*b;
}

/* ---------------- kprintf ---------------- */
#include <stdarg.h>
#include "hal.h"

static void kputc(char c)
{
	if (c == '\n')
		hal_con_putc('\r');
	hal_con_putc(c);
}

void kputs(const char *s)
{
	while (*s)
		kputc(*s++);
}

static void pad_out(int n, char ch)
{
	while (n-- > 0)
		kputc(ch);
}

static void put_num(u32 v, u32 base, int width, int zero, int left, int neg)
{
	char tmp[12];
	int n = 0, total, pad;

	if (v == 0)
		tmp[n++] = '0';
	while (v) {
		u32 d = v % base;

		tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
		v /= base;
	}
	total = n + (neg ? 1 : 0);
	pad = width > total ? width - total : 0;
	if (!left && !zero)
		pad_out(pad, ' ');
	if (neg)
		kputc('-');
	if (!left && zero)
		pad_out(pad, '0');
	while (n)
		kputc(tmp[--n]);
	if (left)
		pad_out(pad, ' ');
}
