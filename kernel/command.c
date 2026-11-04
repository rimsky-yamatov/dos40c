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

static void cmd_dir(const char *arg)
{
	struct dirent de;
	u8 pat[11];
	u16 idx = 0;
	u32 files = 0, bytes = 0;
	char name[13];
	int e;

	if (*arg == 0)
		arg = "*.*";
	e = path_to_fcb(arg, pat);
	if (e < 0) {
		print_error(-e);
		return;
	}
	cmd_vol();
	kputs(" Directory of  A:\\\n\n");
	while (dir_search(pat, ATTR_HIDDEN | ATTR_SYSTEM | ATTR_DIR, &idx,
			  &de) == 0) {
		int n;

		fcb_to_name(de.name, name);
		kprintf("%-8s ", "");
		/* name and extension in separate columns, like DIR */
		for (n = 0; n < 8; n++)
			hal_con_putc(de.name[n]);
		hal_con_putc(' ');
		for (n = 8; n < 11; n++)
			hal_con_putc(de.name[n]);
		if (de.attr & ATTR_DIR)
			kprintf("  <DIR>     ");
		else
			kprintf(" %8lu   ", de.size);
		kprintf("%2d-%02d-%02d\n", (de.date >> 5) & 15, de.date & 31,
			(de.date >> 9) + 80);
		files++;
		bytes += de.size;
	}
	kprintf("%9lu file(s) %10lu bytes\n", files, bytes);
	kprintf("%9s %10lu bytes free\n\n", "",
		fat_free_clusters() * g_vol.spc * g_vol.bps);
}
