/* main.c - kernel initialisation, ported (in reduced form) from
 * DOS/MSINIT.ASM and BIOS/SYSINIT1.ASM. */
#include "dos.h"
#include "farmem.h"
#include "hal.h"
#include "lib.h"

static void install_vectors(void)
{
	u16 cs = kseg();

	far_wr16(0, 0x20 * 4,     (u16)(uintptr_t)int20_entry);
	far_wr16(0, 0x20 * 4 + 2, cs);
	far_wr16(0, 0x21 * 4,     (u16)(uintptr_t)int21_entry);
	far_wr16(0, 0x21 * 4 + 2, cs);
}

static void panic(const char *msg)
{
	kprintf("\nDOS40C: %s\n", msg);
	hal_halt(0x11);
}

void kmain(u32 boot_drive)
{
	int e;

	hal_con_init();
	kputs("\nDOS40C - MS-DOS 4.00 core, ported to C\n");
	kputs("Based on microsoft/MS-DOS v4.0 (MIT License)\n\n");

	e = fat_mount((u8)boot_drive);
	if (e)
		panic("cannot mount boot volume");
	h_init();
	mem_init();
	dos_reset_dta();
	install_vectors();

	kprintf("Boot drive %02x, %lu clusters, %lu KB user memory\n\n",
		boot_drive, g_vol.nclusters, (u32)mem_total() / 64);
	shell_run();
	panic("shell returned");
}
