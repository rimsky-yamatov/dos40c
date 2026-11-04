/* dos_exec.c - load and run .COM programs, ported (in reduced form) from
 * DOS/EXEC.ASM, ABORT.ASM and the PSP setup of DOS/MISC.ASM. */
#include "dos.h"
#include "farmem.h"
#include "lib.h"

#define COM_PARAS    0x1000	/* a .COM program owns a full 64K segment */
#define COM_MAX_SIZE 0xFF00

u16 g_cur_psp;
u8  g_last_ret;

static void build_psp(u8 *psp, u16 seg, const char *tail)
{
	u16 top = (u16)(seg + COM_PARAS);
	size_t len = strlen(tail);

	memset(psp, 0, 256);
	psp[0x00] = 0xCD;		/* INT 20h */
	psp[0x01] = 0x20;
	psp[0x02] = (u8)top;		/* first segment beyond the program */
	psp[0x03] = (u8)(top >> 8);
	psp[0x50] = 0xCD;		/* INT 21h / RETF dispatcher stub */
	psp[0x51] = 0x21;
	psp[0x52] = 0xCB;
	if (len > 126)
		len = 126;
	psp[0x80] = (u8)len;		/* command tail */
	memcpy(psp + 0x81, tail, len);
	psp[0x81 + len] = 0x0D;
}

void dos_terminate(u8 code)
{
	g_last_ret = code;
	if (g_cur_psp == 0)
		return;
	exit_to_kernel();
}

/* Load `path` as a .COM image into a fresh segment and run it.
 * Returns the program's exit code, or a negative DOS error. */
int dos_exec_com(const char *path, const char *tail)
{
	u8 psp[256];
	u8 buf[512];
	u16 seg, largest, off = 0x100;
	u32 size, done = 0;
	int h;

	h = h_open(path, 0);
	if (h < 0)
		return h;
	size = h_size(h);
	if (size > COM_MAX_SIZE) {
		h_close(h);
		return -ERR_NO_MEMORY;
	}
	if (mem_alloc(COM_PARAS, 0, &seg, &largest) != 0) {
		h_close(h);
		return -ERR_NO_MEMORY;
	}

	build_psp(psp, seg, tail);
	far_copy_to(seg, 0, psp, 256);
	while (done < size) {
		int n = h_read(h, buf, sizeof(buf));

		if (n <= 0)
			break;
		far_copy_to(seg, off, buf, (u16)n);
		off = (u16)(off + n);
		done += (u32)n;
	}
	h_close(h);
	h_close_user();

	g_cur_psp = seg;
	g_last_ret = 0;
	run_com(seg);			/* returns on program termination */
	g_cur_psp = 0;

	h_close_user();
	mem_free_owner(seg);
	return g_last_ret;
}
