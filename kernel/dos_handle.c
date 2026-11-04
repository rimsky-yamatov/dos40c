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
