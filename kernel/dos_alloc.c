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

/* free space in front of block i (i == nblk: space after the last block) */
static u16 gap_before(int i, u16 *start)
{
	u16 from = (i == 0) ? MEM_FIRST_SEG : blk[i - 1].seg + blk[i - 1].paras;
	u16 to   = (i == nblk) ? MEM_END_SEG : blk[i].seg;

	*start = from;
	return to - from;
}

u16 mem_largest(void)
{
	u16 best = 0, start;
	int i;

	for (i = 0; i <= nblk; i++) {
		u16 g = gap_before(i, &start);

		if (g > best)
			best = g;
	}
	return best;
}

/* owner == 0 means "owned by the block itself" (used for program images) */
int mem_alloc(u16 paras, u16 owner, u16 *seg, u16 *largest)
{
	int i;

	if (nblk >= MAX_BLOCKS)
		goto fail;
	for (i = 0; i <= nblk; i++) {
		u16 start;

		if (gap_before(i, &start) >= paras) {
			memmove(&blk[i + 1], &blk[i],
				(nblk - i) * sizeof(blk[0]));
			blk[i].seg = start;
			blk[i].paras = paras;
			blk[i].owner = owner ? owner : start;
			nblk++;
			*seg = start;
			return 0;
		}
	}
fail:
	if (largest)
		*largest = mem_largest();
	return ERR_NO_MEMORY;
}

static int find_block(u16 seg)
{
	int i;

	for (i = 0; i < nblk; i++)
		if (blk[i].seg == seg)
			return i;
	return -1;
}

int mem_free(u16 seg)
{
	int i = find_block(seg);

	if (i < 0)
		return ERR_BAD_BLOCK;
	memmove(&blk[i], &blk[i + 1], (nblk - i - 1) * sizeof(blk[0]));
	nblk--;
	return 0;
}
