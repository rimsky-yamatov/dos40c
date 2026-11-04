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

static void fat_set(unsigned c, unsigned v)
{
	unsigned off = c + c / 2;

	if (c & 1) {
		fat[off]     = (uint8_t)((fat[off] & 0x0F) | ((v << 4) & 0xF0));
		fat[off + 1] = (uint8_t)(v >> 4);
	} else {
		fat[off]     = (uint8_t)(v & 0xFF);
		fat[off + 1] = (uint8_t)((fat[off + 1] & 0xF0) | ((v >> 8) & 0x0F));
	}
}

static uint8_t *root_dir(void)
{
	return img + (size_t)(reserved + NFATS * SPF) * SECTOR;
}

static uint8_t *data_area(void)
{
	return root_dir() + ROOT_SECS * SECTOR;
}

static void put16(uint8_t *p, unsigned v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
	put16(p, v & 0xFFFF);
	put16(p + 2, v >> 16);
}

/* "NAME.EXT" -> space padded upper-case 11 byte name */
static void make_name(const char *src, uint8_t out[11])
{
	int i = 0;

	memset(out, ' ', 11);
	for (; *src && *src != '.'; src++)
		if (i < 8)
			out[i++] = (uint8_t)((*src >= 'a' && *src <= 'z') ? *src - 32 : *src);
	if (*src == '.') {
		src++;
		for (i = 8; *src && i < 11; src++)
			out[i++] = (uint8_t)((*src >= 'a' && *src <= 'z') ? *src - 32 : *src);
	}
}

static void add_dirent(const uint8_t name[11], uint8_t attr, unsigned cluster,
		       uint32_t size)
{
	uint8_t *e;

	if (root_index >= ROOT_ENTS)
		die("root directory full", NULL);
	e = root_dir() + root_index++ * 32;
	memcpy(e, name, 11);
	e[11] = attr;
	put16(e + 22, FIXED_TIME);
	put16(e + 24, FIXED_DATE);
	put16(e + 26, cluster);
	put32(e + 28, size);
}

static void add_file(const char *spec)
{
	char path[512], *colon;
	const char *name, *base;
	uint8_t n11[11], *data;
	size_t len, nclu, i;
	unsigned first;

	strncpy(path, spec, sizeof(path) - 1);
	path[sizeof(path) - 1] = 0;
	colon = strrchr(path, ':');
	if (colon) {
		*colon = 0;
		name = colon + 1;
	} else {
		base = strrchr(path, '/');
		name = base ? base + 1 : path;
	}
	data = read_file(path, &len);
	make_name(name, n11);
	nclu = (len + SECTOR - 1) / SECTOR;
	if (next_cluster + nclu > TOTAL_SECS)
		die("image full", path);
	first = nclu ? next_cluster : 0;
	for (i = 0; i < nclu; i++) {
		unsigned c = next_cluster + (unsigned)i;

		fat_set(c, i + 1 == nclu ? 0xFFF : c + 1);
		memcpy(data_area() + (size_t)(c - 2) * SECTOR, data + i * SECTOR,
		       len - i * SECTOR < SECTOR ? len - i * SECTOR : SECTOR);
	}
	next_cluster += (unsigned)nclu;
	add_dirent(n11, 0x20, first, (uint32_t)len);
	free(data);
}
