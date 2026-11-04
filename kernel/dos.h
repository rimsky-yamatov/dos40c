/* dos.h - internal DOS kernel interfaces (ported from the MS-DOS 4.0 DOS sources) */
#ifndef DOS40C_DOS_H
#define DOS40C_DOS_H

#include "types.h"

#define DOS_VER_MAJOR 4
#define DOS_VER_MINOR 0

/* DOS error codes (see DOS/ERROR in the original) */
#define ERR_INVALID_FUNC    1
#define ERR_FILE_NOT_FOUND  2
#define ERR_PATH_NOT_FOUND  3
#define ERR_TOO_MANY_OPEN   4
#define ERR_ACCESS_DENIED   5
#define ERR_INVALID_HANDLE  6
#define ERR_NO_MEMORY       8
#define ERR_BAD_BLOCK       9
#define ERR_INVALID_DRIVE   15
#define ERR_NO_MORE_FILES   18
#define ERR_READ_FAULT      30

/* directory entry attributes */
#define ATTR_RDONLY  0x01
#define ATTR_HIDDEN  0x02
#define ATTR_SYSTEM  0x04
#define ATTR_VOLUME  0x08
#define ATTR_DIR     0x10
#define ATTR_ARCHIVE 0x20


/* ---------- ALLOC.ASM: memory arena (paragraph granularity) ---------- */
#define MEM_FIRST_SEG 0x2000	/* user memory: 2000:0000 .. 9000:0000 */
#define MEM_END_SEG   0x9000

void mem_init(void);
int  mem_alloc(u16 paras, u16 owner, u16 *seg, u16 *largest);
int  mem_free(u16 seg);
int  mem_resize(u16 seg, u16 paras, u16 *maxparas);
void mem_free_owner(u16 owner);
u16  mem_largest(void);
u16  mem_total(void);

/* ---------- FAT.ASM / DISK.ASM: FAT12 volume ---------- */
#define FAT_MAX_SECTORS 12	/* sectors of one FAT that we cache */
#define EOC_MIN 0xFF8

struct fat_vol {
	u8  drive;
	u16 bps;		/* bytes per sector (always 512) */
	u8  spc;		/* sectors per cluster */
	u16 reserved;
	u8  nfats;
	u16 rootents;
	u32 totsec;
	u16 spf;
	u32 fat_lba, root_lba, data_lba;
	u32 nclusters;
	char label[12];
};
extern struct fat_vol g_vol;

int        fat_mount(u8 drive);
u16        fat_next(u16 cluster);
u32        fat_cluster_lba(u16 cluster);
const u8  *fat_sector(u32 lba);
u32        fat_free_clusters(void);

/* ---------- DIR.ASM: root directory ---------- */
struct dirent {
	u8  name[11];		/* 8.3, space padded, no dot */
	u8  attr;
	u16 time;
	u16 date;
	u16 cluster;
	u32 size;
};

#define DIR_OK    0
#define DIR_SKIP  1
#define DIR_END   2
#define DIR_ERROR (-1)

int  path_to_fcb(const char *path, u8 fcb[11]);
int  dir_get(u16 index, struct dirent *de);
int  dir_find(const u8 fcb[11], struct dirent *out);
int  dir_search(const u8 pattern[11], u8 attrmask, u16 *index,
		struct dirent *out);
void fcb_to_name(const u8 fcb[11], char out[13]);

/* ---------- HANDLE.ASM / OPEN.ASM: file handles ---------- */
#define SFT_MAX      20
#define H_FIRST_FILE 5
#define H_TYPE_FILE  0
#define H_TYPE_CON   1
#define H_TYPE_NUL   2

void h_init(void);
int  h_open(const char *path, u8 mode);
int  h_close(int h);
int  h_type(int h);
int  h_read(int h, void *buf, u16 n);
i32  h_seek(int h, i32 off, u8 whence);
u32  h_size(int h);
void h_close_user(void);

#endif
