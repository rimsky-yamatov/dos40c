/* test_core.c - host tests for alloc / FAT12 / dir / handle modules.
 * BOOT_BIN is the real build/boot.bin so the BPB is checked too. */
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
	static uint8_t kern[1500], big[5000];
	size_t i;

	for (i = 0; i < sizeof(kern); i++) kern[i] = (uint8_t)i;
	for (i = 0; i < sizeof(big); i++) big[i] = (uint8_t)(i * 7 + (i >> 8));
	write_file("/tmp/t_kern.bin", kern, sizeof(kern));
	write_file("/tmp/t_big.dat", big, sizeof(big));
	write_file("/tmp/t_small.txt", "hello\r\nworld\r\n", 14);
	write_file("/tmp/t_empty.dat", "", 0);
	{
		char *a[] = { "mkfat", "/tmp/t.img", BOOT_BIN, "/tmp/t_kern.bin",
			      "/tmp/t_big.dat:BIG.DAT", "/tmp/t_small.txt:readme.txt",
			      "/tmp/t_empty.dat:EMPTY.BIN" };
		mkfat_main(7, a);
	}
}

static void test_alloc(void)
{
	u16 a, b, c, big, largest;

	mem_init();
	CHECK(mem_total() == MEM_END_SEG - MEM_FIRST_SEG);
	CHECK(mem_alloc(0x1000, 0, &a, &largest) == 0);
	CHECK(a == MEM_FIRST_SEG);
	CHECK(mem_alloc(0x100, 0, &b, &largest) == 0);
	CHECK(b == a + 0x1000);
	CHECK(mem_alloc(0x200, 0x77, &c, &largest) == 0);
	CHECK(c == b + 0x100);
	CHECK(mem_free(b) == 0);
	CHECK(mem_alloc(0x80, 0, &b, &largest) == 0);	/* reuses the hole */
	CHECK(b == a + 0x1000);
	CHECK(mem_free(0x1234) == ERR_BAD_BLOCK);
	CHECK(mem_alloc(0xFFFF, 0, &big, &largest) == ERR_NO_MEMORY);
	CHECK(largest == MEM_END_SEG - (c + 0x200));
	CHECK(mem_resize(c, 0x400, &largest) == 0);
	CHECK(mem_resize(c, 0xF000, &largest) == ERR_NO_MEMORY);
	mem_free_owner(0x77);
	CHECK(mem_free(c) == ERR_BAD_BLOCK);		/* already gone */
	mem_free_owner(a);				/* owner defaults to seg */
	CHECK(mem_free(a) == ERR_BAD_BLOCK);
}

static void test_mount(void)
{
	CHECK(host_disk_open("/tmp/t.img") == 0);
	CHECK(fat_mount(0) == 0);
	CHECK(g_vol.reserved == 4);		/* 1 boot + 3 kernel sectors */
	CHECK(g_vol.spf == 9 && g_vol.nfats == 2 && g_vol.rootents == 224);
	CHECK(strcmp(g_vol.label, "DOS40C") == 0);
	CHECK(g_vol.data_lba == 4 + 18 + 14);
	CHECK(g_vol.nclusters == 2880 - 36);
	CHECK(fat_free_clusters() == g_vol.nclusters - 11);	/* 10 + 1 + 0 */
}

static void test_path(void)
{
	u8 f[11];

	CHECK(path_to_fcb("big.dat", f) == 0 && !memcmp(f, "BIG     DAT", 11));
	CHECK(path_to_fcb("A:\\Big.Dat", f) == 0 && !memcmp(f, "BIG     DAT", 11));
	CHECK(path_to_fcb("*.*", f) == 0 && !memcmp(f, "???????????", 11));
	CHECK(path_to_fcb("b*.d?t", f) == 0 && !memcmp(f, "B???????D?T", 11));
	CHECK(path_to_fcb("toolongname.x", f) < 0);
	CHECK(path_to_fcb("sub\\x.txt", f) == -ERR_PATH_NOT_FOUND);
	CHECK(path_to_fcb("B:x", f) == -ERR_INVALID_DRIVE);
	CHECK(path_to_fcb("", f) < 0);
}

static void test_dir(void)
{
	struct dirent de;
	u8 pat[11], f[11];
	u16 idx = 0;
	int n = 0;
	char name[13];

	path_to_fcb("*.*", pat);
	while (dir_search(pat, 0, &idx, &de) == 0)
		n++;
	CHECK(n == 3);				/* BIG.DAT README.TXT EMPTY.BIN */

	idx = 0;
	CHECK(dir_search(pat, ATTR_VOLUME, &idx, &de) == 0);
	CHECK(de.attr & ATTR_VOLUME);		/* label shows up when requested */

	path_to_fcb("readme.txt", f);
	CHECK(dir_find(f, &de) == 0);
	CHECK(de.size == 14);
	fcb_to_name(de.name, name);
	CHECK(strcmp(name, "README.TXT") == 0);

	path_to_fcb("nothere.dat", f);
	CHECK(dir_find(f, &de) == -ERR_FILE_NOT_FOUND);

	idx = 0;
	path_to_fcb("*.dat", pat);
	CHECK(dir_search(pat, 0, &idx, &de) == 0);
	CHECK(dir_search(pat, 0, &idx, &de) == -ERR_NO_MORE_FILES);
}

static void test_files(void)
{
	static uint8_t expect[5000], got[6000];
	int h, n;
	size_t i;
	char buf[32];

	h_init();
	for (i = 0; i < sizeof(expect); i++)
		expect[i] = (uint8_t)(i * 7 + (i >> 8));

	h = h_open("big.dat", 0);
	CHECK(h >= H_FIRST_FILE);
	CHECK(h_size(h) == 5000);
	n = h_read(h, got, 6000);		/* more than the file holds */
	CHECK(n == 5000);
	CHECK(memcmp(got, expect, 5000) == 0);
	CHECK(h_read(h, got, 10) == 0);		/* at EOF */
	CHECK(h_seek(h, 4090, 0) == 4090);
	CHECK(h_read(h, got, 20) == 20 && !memcmp(got, expect + 4090, 20));
	CHECK(h_seek(h, -10, 2) == 4990);
	CHECK(h_read(h, got, 100) == 10 && !memcmp(got, expect + 4990, 10));
	CHECK(h_seek(h, -100, 1) == 4900);
	CHECK(h_seek(h, -1, 0) < 0);
	CHECK(h_close(h) == 0);
	CHECK(h_close(h) == -ERR_INVALID_HANDLE);

	h = h_open("README.TXT", 0);
	CHECK(h_read(h, buf, 5) == 5 && !memcmp(buf, "hello", 5));
	CHECK(h_read(h, buf, 3) == 3 && !memcmp(buf, "\r\nw", 3));
	h_close(h);

	h = h_open("empty.bin", 0);
	CHECK(h >= 0 && h_size(h) == 0 && h_read(h, buf, 4) == 0);
	h_close(h);

	CHECK(h_open("nope.txt", 0) == -ERR_FILE_NOT_FOUND);
	CHECK(h_open("*.txt", 0) == -ERR_FILE_NOT_FOUND);
	CHECK(h_open("big.dat", 1) == -ERR_ACCESS_DENIED);   /* write mode */
	CHECK(h_type(1) == H_TYPE_CON && h_type(3) == H_TYPE_NUL);
	CHECK(h_close(1) == 0 && h_type(1) == H_TYPE_CON);   /* std handles stay */

	for (i = 0; i < SFT_MAX; i++)
		(void)h_open("big.dat", 0);
	CHECK(h_open("big.dat", 0) == -ERR_TOO_MANY_OPEN);
	h_close_user();
	CHECK(h_open("big.dat", 0) == H_FIRST_FILE);
}

int main(void)
{
	make_fixture();
	test_alloc();
	test_mount();
	test_path();
	test_dir();
	test_files();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
