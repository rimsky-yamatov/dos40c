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

/* ---------------- internal commands ---------------- */
static void cmd_ver(void)
{
	kprintf("\nMS-DOS Version %d.%02d (DOS40C, C port)\n\n",
		DOS_VER_MAJOR, DOS_VER_MINOR);
}

static void cmd_echo(const char *arg)
{
	if (*arg == 0)
		kprintf("ECHO is %s\n", echo_on ? "on" : "off");
	else if (streq_nocase(arg, "ON"))
		echo_on = 1;
	else if (streq_nocase(arg, "OFF"))
		echo_on = 0;
	else
		kprintf("%s\n", arg);
}

static void cmd_vol(void)
{
	if (g_vol.label[0])
		kprintf(" Volume in drive A is %s\n", g_vol.label);
	else
		kputs(" Volume in drive A has no label\n");
}

static void cmd_mem(void)
{
	u32 total = (u32)mem_total() * 16, large = (u32)mem_largest() * 16;

	kprintf("\n%8lu bytes total user memory\n", total);
	kprintf("%8lu bytes free (largest block)\n\n", large);
}

static void cmd_help(void)
{
	kputs("Internal commands:\n"
	      "  DIR [pattern]  list files      TYPE file   show a text file\n"
	      "  ECHO [text]    print / ON|OFF  VER         show version\n"
	      "  VOL            volume label    MEM         memory info\n"
	      "  CLS            clear screen    PAUSE       wait for a key\n"
	      "  REM text       comment         HALT        power off\n"
	      "Any NAME.COM or NAME.BAT on the disk can be run by typing NAME.\n");
}
