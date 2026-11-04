/* hal_host.c - file-backed disk for running the kernel core on the host */
#include <stdio.h>
#include "../kernel/hal.h"

static FILE *img;

int host_disk_open(const char *path)
{
	img = fopen(path, "rb");
	return img ? 0 : -1;
}

void hal_set_geometry(u8 drive, u16 spt, u16 heads)
{
	(void)drive; (void)spt; (void)heads;
}

int hal_read_sector(u8 drive, u32 lba, void *buf)
{
	(void)drive;
	if (!img || fseek(img, (long)lba * 512, SEEK_SET))
		return -1;
	return fread(buf, 512, 1, img) == 1 ? 0 : -1;
}
