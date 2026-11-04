/* dos_alloc.c - memory arena, ported from DOS/ALLOC.ASM
 *
 * The original chains MCBs inside the arena.  Here a small sorted table
 * of blocks is kept outside the arena, which keeps user memory clean.
 */
#include "dos.h"
#include "lib.h"

#define MAX_BLOCKS 32

struct block {
	u16 seg;
	u16 paras;
	u16 owner;
};

static struct block blk[MAX_BLOCKS];
static int nblk;

void mem_init(void)
{
	nblk = 0;
}

u16 mem_total(void)
{
	return MEM_END_SEG - MEM_FIRST_SEG;
}
