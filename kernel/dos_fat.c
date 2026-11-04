/* dos_fat.c - FAT12 volume access, ported from DOS/FAT.ASM and DISK.ASM */
#include "dos.h"
#include "hal.h"
#include "lib.h"

struct fat_vol g_vol;

static u8  fat_tab[FAT_MAX_SECTORS * 512];
static u8  sec_buf[512];
static u32 sec_lba;
static u8  sec_valid;

static u16 rd16(const u8 *p)
{
	return (u16)(p[0] | (p[1] << 8));
}

static u32 rd32(const u8 *p)
{
	return (u32)rd16(p) | ((u32)rd16(p + 2) << 16);
}

/* one-sector read cache (the original used BUF.ASM for this) */
const u8 *fat_sector(u32 lba)
{
	if (sec_valid && sec_lba == lba)
		return sec_buf;
	sec_valid = 0;
	if (hal_read_sector(g_vol.drive, lba, sec_buf) != 0)
		return NULL;
	sec_lba = lba;
	sec_valid = 1;
	return sec_buf;
}

u32 fat_cluster_lba(u16 cluster)
{
	return g_vol.data_lba + (u32)(cluster - 2) * g_vol.spc;
}
