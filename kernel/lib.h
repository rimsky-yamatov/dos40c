/* lib.h - tiny freestanding C library */
#ifndef DOS40C_LIB_H
#define DOS40C_LIB_H

#include "types.h"

#ifdef HOST_TEST
#include <string.h>
#else
void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
void  *memset(void *dst, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
#endif

/* printf subset: %c %s %d %u %x %lu %ld %lx, width, '0' and '-' flags.
   '\n' is expanded to CR LF. */
void kprintf(const char *fmt, ...);
void kputs(const char *s);

#endif
