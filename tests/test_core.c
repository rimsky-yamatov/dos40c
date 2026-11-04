/* test_core.c - host tests for alloc / FAT12 / dir / handle modules */
#define MKFAT_NO_MAIN
#include "../tools/mkfat.c"
#include "../kernel/dos.h"

int host_disk_open(const char *path);

static int failures, checks;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
	printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void write_file(const char *p, const void *d, size_t n)
{
	FILE *f = fopen(p, "wb");

	fwrite(d, 1, n, f);
	fclose(f);
}

static void make_fixture(void)
{
	static uint8_t boot[512], kern[1500], big[5000];
	size_t i;

	boot[510] = 0x55; boot[511] = 0xAA;
	for (i = 0; i < sizeof(kern); i++) kern[i] = (uint8_t)i;
	for (i = 0; i < sizeof(big); i++) big[i] = (uint8_t)(i * 7 + (i >> 8));
	write_file("/tmp/t_boot.bin", boot, sizeof(boot));
	write_file("/tmp/t_kern.bin", kern, sizeof(kern));
	write_file("/tmp/t_big.dat", big, sizeof(big));
	write_file("/tmp/t_small.txt", "hello\r\nworld\r\n", 14);
	write_file("/tmp/t_empty.dat", "", 0);
	{
		char *a[] = { "mkfat", "/tmp/t.img", "/tmp/t_boot.bin", "/tmp/t_kern.bin",
			      "/tmp/t_big.dat:BIG.DAT", "/tmp/t_small.txt:readme.txt",
			      "/tmp/t_empty.dat:EMPTY.BIN" };
		mkfat_main(7, a);
	}
}
