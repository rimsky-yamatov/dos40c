/* dos.h - internal DOS kernel interfaces (ported from MS-DOS 4.0 DOS/*.ASM) */
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

#endif
