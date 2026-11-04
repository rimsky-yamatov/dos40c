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
