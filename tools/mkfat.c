/*
 * mkfat.c - build a bootable 1.44MB FAT12 floppy image for DOS40C
 *
 *   mkfat OUT.IMG BOOT.BIN KERNEL.BIN [FILE[:NAME.EXT]]...
 *
 * Layout: sector 0 boot sector, sectors 1..R-1 kernel image (the
 * reserved area), then 2 FATs, 224 root entries and the data area.
 * Timestamps are fixed so that images are reproducible.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SECTOR      512
#define TOTAL_SECS  2880
#define SPF         9
#define NFATS       2
#define ROOT_ENTS   224
#define ROOT_SECS   (ROOT_ENTS * 32 / SECTOR)
#define MAX_KERNEL_SECS 120

#define FIXED_DATE ((1988 - 1980) << 9 | 6 << 5 | 17)	/* 1988-06-17 */
#define FIXED_TIME 0

static uint8_t img[TOTAL_SECS * SECTOR];
static uint8_t fat[SPF * SECTOR];
static unsigned next_cluster = 2;
static unsigned root_index;
static unsigned reserved;

static void die(const char *msg, const char *arg)
{
	fprintf(stderr, "mkfat: %s%s%s\n", msg, arg ? ": " : "", arg ? arg : "");
	exit(1);
}

static uint8_t *read_file(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	uint8_t *buf;
	long n;

	if (!f)
		die("cannot open", path);
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = malloc(n ? n : 1);
	if (!buf || fread(buf, 1, n, f) != (size_t)n)
		die("read failed", path);
	fclose(f);
	*len = (size_t)n;
	return buf;
}
