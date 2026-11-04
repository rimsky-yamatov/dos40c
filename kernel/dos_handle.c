/* dos_handle.c - file handle table, ported from DOS/HANDLE.ASM, OPEN.ASM,
 * CLOSE.ASM.  Files are read-only; handles 0-4 are the standard devices. */
#include "dos.h"
#include "lib.h"

struct sft {
	u8  used;
	u8  type;
	struct dirent de;
	u32 pos;
};

static struct sft sft[SFT_MAX];

void h_init(void)
{
	int i;

	memset(sft, 0, sizeof(sft));
	for (i = 0; i < H_FIRST_FILE; i++) {
		sft[i].used = 1;
		sft[i].type = (i < 3) ? H_TYPE_CON : H_TYPE_NUL;
	}
}

static struct sft *get(int h)
{
	if (h < 0 || h >= SFT_MAX || !sft[h].used)
		return NULL;
	return &sft[h];
}

int h_type(int h)
{
	struct sft *f = get(h);

	return f ? f->type : -ERR_INVALID_HANDLE;
}

u32 h_size(int h)
{
	struct sft *f = get(h);

	return f ? f->de.size : 0;
}

/* returns handle (>= 5) or -DOS error */
int h_open(const char *path, u8 mode)
{
	u8 fcb[11];
	struct dirent de;
	int r, h, i;

	r = path_to_fcb(path, fcb);
	if (r < 0)
		return r;
	for (i = 0; i < 11; i++)
		if (fcb[i] == '?')
			return -ERR_FILE_NOT_FOUND;
	if ((mode & 7) != 0)		/* write access: not supported */
		return -ERR_ACCESS_DENIED;
	r = dir_find(fcb, &de);
	if (r < 0)
		return r;
	if (de.attr & ATTR_DIR)
		return -ERR_ACCESS_DENIED;
	for (h = H_FIRST_FILE; h < SFT_MAX; h++) {
		if (!sft[h].used) {
			sft[h].used = 1;
			sft[h].type = H_TYPE_FILE;
			sft[h].de = de;
			sft[h].pos = 0;
			return h;
		}
	}
	return -ERR_TOO_MANY_OPEN;
}

int h_close(int h)
{
	struct sft *f = get(h);

	if (!f)
		return -ERR_INVALID_HANDLE;
	if (h >= H_FIRST_FILE)
		f->used = 0;
	return 0;
}

void h_close_user(void)
{
	int h;

	for (h = H_FIRST_FILE; h < SFT_MAX; h++)
		sft[h].used = 0;
}

/* Read up to n bytes at the current position.  Returns count or -error. */
int h_read(int h, void *buf, u16 n)
{
	struct sft *f = get(h);
	u32 csz = (u32)g_vol.spc * g_vol.bps;
	u32 remain;
	u16 got = 0;

	if (!f || f->type != H_TYPE_FILE)
		return -ERR_INVALID_HANDLE;
	remain = f->de.size > f->pos ? f->de.size - f->pos : 0;
	if (n > remain)
		n = (u16)remain;

	while (got < n) {
		u32 cidx = f->pos / csz;
		u32 in_c = f->pos % csz;
		u16 c = f->de.cluster, boff, chunk;
		u32 i, lba;
		const u8 *s;

		for (i = 0; i < cidx; i++) {
			c = fat_next(c);
			if (c < 2 || c >= EOC_MIN)
				return got ? (int)got : -ERR_READ_FAULT;
		}
		lba = fat_cluster_lba(c) + in_c / g_vol.bps;
		boff = (u16)(in_c % g_vol.bps);
		s = fat_sector(lba);
		if (!s)
			return got ? (int)got : -ERR_READ_FAULT;
		chunk = (u16)(g_vol.bps - boff);
		if (chunk > (u16)(n - got))
			chunk = (u16)(n - got);
		memcpy((u8 *)buf + got, s + boff, chunk);
		f->pos += chunk;
		got = (u16)(got + chunk);
	}
	return got;
}

/* whence: 0 = start, 1 = current, 2 = end.  Returns new position or -error */
i32 h_seek(int h, i32 off, u8 whence)
{
	struct sft *f = get(h);
	i32 base, np;

	if (!f || f->type != H_TYPE_FILE)
		return -ERR_INVALID_HANDLE;
	switch (whence) {
	case 0: base = 0; break;
	case 1: base = (i32)f->pos; break;
	case 2: base = (i32)f->de.size; break;
	default: return -ERR_INVALID_FUNC;
	}
	np = base + off;
	if (np < 0)
		return -ERR_INVALID_FUNC;
	f->pos = (u32)np;
	return np;
}
