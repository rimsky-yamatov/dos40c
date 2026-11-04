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

static void cmd_type(const char *arg)
{
	char buf[128];
	int h, n, i;

	if (*arg == 0) {
		kputs("Required parameter missing\n");
		return;
	}
	h = h_open(arg, 0);
	if (h < 0) {
		print_error(-h);
		return;
	}
	while ((n = h_read(h, buf, sizeof(buf))) > 0) {
		for (i = 0; i < n; i++) {
			if (buf[i] == 0x1A)		/* Ctrl-Z = EOF */
				goto out;
			hal_con_putc(buf[i]);
		}
	}
out:
	h_close(h);
}

static void cmd_pause(void)
{
	kputs("Press any key to continue . . .");
	(void)con_getch();
	kputs("\n");
}

static void cmd_halt(void)
{
	kputs("DOS40C: HALT\n");
	hal_halt(0x10);			/* QEMU exit status 33 */
}

/* ---------------- running programs and batch files ---------------- */
static int file_exists(const char *name)
{
	u8 fcb[11];
	struct dirent de;

	return path_to_fcb(name, fcb) == 0 && dir_find(fcb, &de) == 0;
}

static int run_batch(const char *name);

/* try NAME.COM, then NAME.BAT.  Returns 1 if something was run. */
static int run_external(const char *cmd, const char *tail)
{
	char path[40];
	size_t n = strlen(cmd);
	int has_ext = 0, i, r;

	if (n > 30)
		return 0;
	for (i = 0; i < (int)n; i++)
		if (cmd[i] == '.')
			has_ext = 1;
	memcpy(path, cmd, n + 1);
	if (has_ext) {
		if (!file_exists(path))
			return 0;
	} else {
		memcpy(path + n, ".COM", 5);
		if (!file_exists(path)) {
			memcpy(path + n, ".BAT", 5);
			if (!file_exists(path))
				return 0;
		}
	}
	if (strcmp(path + strlen(path) - 4, ".BAT") == 0) {
		run_batch(path);
		return 1;
	}
	r = dos_exec_com(path, tail);
	if (r < 0)
		print_error(-r);
	return 1;
}

/* Execute one command line.  The line is modified in place. */
int shell_exec_line(char *line)
{
	char cmd[16], *arg;
	int n = 0;

	while (*line == ' ' || *line == '\t')
		line++;
	if (*line == '@')
		line++;
	if (*line == 0 || *line == ':')
		return 0;

	while (!is_sep(line[n]) && n < (int)sizeof(cmd) - 1) {
		cmd[n] = upper(line[n]);
		n++;
	}
	cmd[n] = 0;
	arg = line + n;
	if (*arg == ' ' || *arg == '\t')
		arg++;
	/* keep a leading '/' (switch) in the argument, drop blanks */
	while (*arg == ' ' || *arg == '\t')
		arg++;

	if (!strcmp(cmd, "REM"))		return 0;
	if (!strcmp(cmd, "ECHO"))		cmd_echo(arg);
	else if (!strcmp(cmd, "VER"))		cmd_ver();
	else if (!strcmp(cmd, "DIR"))		cmd_dir(arg);
	else if (!strcmp(cmd, "TYPE"))		cmd_type(arg);
	else if (!strcmp(cmd, "VOL"))		cmd_vol();
	else if (!strcmp(cmd, "MEM"))		cmd_mem();
	else if (!strcmp(cmd, "HELP"))		cmd_help();
	else if (!strcmp(cmd, "CLS"))		hal_con_cls();
	else if (!strcmp(cmd, "PAUSE"))		cmd_pause();
	else if (!strcmp(cmd, "HALT") || !strcmp(cmd, "SHUTDOWN"))
		cmd_halt();
	else if (!run_external(cmd, line + n))
		kputs("Bad command or file name\n");
	return 0;
}
