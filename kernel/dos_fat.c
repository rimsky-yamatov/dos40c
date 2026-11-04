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

u16 fat_next(u16 c)
{
	u32 off = c + (c >> 1);
	u16 v = (u16)(fat_tab[off] | (fat_tab[off + 1] << 8));

	return (c & 1) ? (u16)(v >> 4) : (u16)(v & 0x0FFF);
}

u32 fat_free_clusters(void)
{
	u32 n = 0, c;

	for (c = 2; c < g_vol.nclusters + 2; c++)
		if (fat_next((u16)c) == 0)
			n++;
	return n;
}

/* Parse the BPB in sector 0 and load FAT #1.  Returns 0 or a DOS error. */
int fat_mount(u8 drive)
{
	const u8 *s;
	u32 root_secs, data_secs;
	u16 i;

	memset(&g_vol, 0, sizeof(g_vol));
	sec_valid = 0;
	g_vol.drive = drive;
	s = fat_sector(0);
	if (!s)
		return ERR_READ_FAULT;
	if (rd16(s + 510) != 0xAA55)
		return ERR_INVALID_DRIVE;

	g_vol.bps      = rd16(s + 11);
	g_vol.spc      = s[13];
	g_vol.reserved = rd16(s + 14);
	g_vol.nfats    = s[16];
	g_vol.rootents = rd16(s + 17);
	g_vol.totsec   = rd16(s + 19) ? rd16(s + 19) : rd32(s + 32);
	g_vol.spf      = rd16(s + 22);
	if (g_vol.bps != 512 || g_vol.spc == 0 || g_vol.nfats == 0 ||
	    g_vol.spf == 0 || g_vol.spf > FAT_MAX_SECTORS)
		return ERR_INVALID_DRIVE;

	hal_set_geometry(drive, rd16(s + 24), rd16(s + 26));
	if (s[38] == 0x29)
		memcpy(g_vol.label, s + 43, 11);
	g_vol.label[11] = 0;
	for (i = 10; i > 0 && g_vol.label[i] == ' '; i--)
		g_vol.label[i] = 0;

	g_vol.fat_lba  = g_vol.reserved;
	g_vol.root_lba = g_vol.fat_lba + (u32)g_vol.nfats * g_vol.spf;
	root_secs      = ((u32)g_vol.rootents * 32 + 511) / 512;
	g_vol.data_lba = g_vol.root_lba + root_secs;
	data_secs      = g_vol.totsec - g_vol.data_lba;
	g_vol.nclusters = data_secs / g_vol.spc;
	if (g_vol.nclusters >= 4085)	/* FAT16: not supported */
		return ERR_INVALID_DRIVE;

	/* load FAT #1 into memory; FAT12 is at most 6 KB */
	for (i = 0; i < g_vol.spf; i++) {
		s = fat_sector(g_vol.fat_lba + i);
		if (!s)
			return ERR_READ_FAULT;
		memcpy(fat_tab + (u32)i * 512, s, 512);
	}
	return 0;
}
