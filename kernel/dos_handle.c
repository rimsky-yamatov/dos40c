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
