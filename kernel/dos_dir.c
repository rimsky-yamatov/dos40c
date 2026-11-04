/* dos_dir.c - root directory and path parsing, ported from DOS/DIR.ASM,
 * PATH.ASM and SEARCH.ASM.  Only the root directory of drive A: exists. */
#include "dos.h"
#include "lib.h"

static u16 rd16(const u8 *p)
{
	return (u16)(p[0] | (p[1] << 8));
}

static u32 rd32(const u8 *p)
{
	return (u32)rd16(p) | ((u32)rd16(p + 2) << 16);
}

/* Read directory slot `index`. */
int dir_get(u16 index, struct dirent *de)
{
	const u8 *s, *e;

	if (index >= g_vol.rootents)
		return DIR_END;
	s = fat_sector(g_vol.root_lba + index / 16);
	if (!s)
		return DIR_ERROR;
	e = s + (index % 16) * 32;
	if (e[0] == 0x00)
		return DIR_END;
	if (e[0] == 0xE5 || (e[11] & 0x0F) == 0x0F)
		return DIR_SKIP;
	memcpy(de->name, e, 11);
	de->attr    = e[11];
	de->time    = rd16(e + 22);
	de->date    = rd16(e + 24);
	de->cluster = rd16(e + 26);
	de->size    = rd32(e + 28);
	return DIR_OK;
}
