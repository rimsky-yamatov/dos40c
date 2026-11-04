/* command.c - built-in command interpreter, a compact replacement for
 * CMD/COMMAND (COMMAND1.ASM, TCODE.ASM, TBATCH.ASM ...). */
#include "dos.h"
#include "hal.h"
#include "lib.h"

static int echo_on = 1;
static int batch_depth;

static char upper(char c)
{
	return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

static int is_sep(char c)
{
	return c == ' ' || c == '\t' || c == '/' || c == '\0';
}

static int streq_nocase(const char *a, const char *b)
{
	while (*a && upper(*a) == upper(*b)) {
		a++;
		b++;
	}
	return *a == 0 && *b == 0;
}

static void print_error(int e)
{
	switch (e) {
	case ERR_FILE_NOT_FOUND: kputs("File not found\n"); break;
	case ERR_PATH_NOT_FOUND: kputs("Path not found\n"); break;
	case ERR_ACCESS_DENIED:  kputs("Access denied\n"); break;
	case ERR_NO_MEMORY:      kputs("Insufficient memory\n"); break;
	case ERR_INVALID_DRIVE:  kputs("Invalid drive specification\n"); break;
	default: kprintf("DOS error %d\n", e); break;
	}
}
