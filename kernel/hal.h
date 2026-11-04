/* hal.h - hardware abstraction (BIOS / serial / disk) */
#ifndef DOS40C_HAL_H
#define DOS40C_HAL_H

#include "types.h"

/* console: BIOS INT 10h/16h mirrored to / read from COM1 */
void hal_con_init(void);
void hal_con_putc(char c);
int  hal_con_kbhit(void);
int  hal_con_getkey(void);	/* blocking; returns ASCII, 0 for extended keys */
void hal_con_cls(void);

/* disk: 512-byte sector access through INT 13h */
void hal_set_geometry(u8 drive, u16 spt, u16 heads);
int  hal_read_sector(u8 drive, u32 lba, void *buf);

/* machine control */
void hal_halt(int code) __attribute__((noreturn));

#endif
