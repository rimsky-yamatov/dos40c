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
