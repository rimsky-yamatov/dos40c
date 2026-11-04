/* dos_dispatch.c - INT 21h function dispatcher, ported from
 * DOS/DISPATCH.ASM, MSDISP.ASM and the individual service routines. */
#include "dos.h"
#include "farmem.h"
#include "hal.h"
#include "lib.h"

struct regs g_regs;
extern u8 g_last_ret;

#define AH(r) ((u8)((r)->ax >> 8))
#define AL(r) ((u8)((r)->ax))
#define DL(r) ((u8)((r)->dx))

static u16 dta_seg, dta_off;
static u8  kdta[128];

void dos_set_dta(u16 seg, u16 off)
{
	dta_seg = seg;
	dta_off = off;
}

void dos_reset_dta(void)
{
	dta_seg = kseg();
	dta_off = (u16)(uintptr_t)kdta;
}

static void ret_ok(struct regs *r)
{
	r->flags &= (u16)~FLAG_CF;
}

static void ret_err(struct regs *r, int err)
{
	r->ax = (u16)err;
	r->flags |= FLAG_CF;
}

/* r->ds:r->dx path -> kernel buffer */
static void get_path(struct regs *r, char *buf, u16 max)
{
	far_get_str(buf, r->ds, r->dx, max);
}

/* ---- 01h/02h/06h/07h/08h/09h/0Ah/0Bh: character I/O ---- */
static void fn_char_io(struct regs *r, u8 ah)
{
	switch (ah) {
	case 0x01: {			/* read with echo */
		int c = con_getch();

		con_putc((char)c);
		r->ax = (u16)((r->ax & 0xFF00) | (u8)c);
		break;
	}
	case 0x02:			/* write character */
		con_putc((char)DL(r));
		break;
	case 0x06:			/* direct console I/O */
		if (DL(r) != 0xFF) {
			con_putc((char)DL(r));
		} else if (hal_con_kbhit()) {
			r->ax = (u16)((r->ax & 0xFF00) | (u8)con_getch());
			r->flags &= (u16)~FLAG_ZF;
		} else {
			r->ax &= 0xFF00;
			r->flags |= FLAG_ZF;
		}
		break;
	case 0x07:			/* read, no echo */
	case 0x08:
		r->ax = (u16)((r->ax & 0xFF00) | (u8)con_getch());
		break;
	case 0x0B:			/* input status */
		r->ax = (u16)((r->ax & 0xFF00) | (hal_con_kbhit() ? 0xFF : 0));
		break;
	}
}

static void fn_print_string(struct regs *r)	/* 09h */
{
	u16 off = r->dx;
	char c;

	while ((c = (char)far_rd8(r->ds, off++)) != '$')
		con_putc(c);
}

static void fn_buffered_input(struct regs *r)	/* 0Ah */
{
	char line[256];
	u8 max = far_rd8(r->ds, r->dx);
	int n;

	if (max == 0)
		return;
	n = con_readline(line, max);
	far_wr8(r->ds, (u16)(r->dx + 1), (u8)n);
	far_copy_to(r->ds, (u16)(r->dx + 2), line, (u16)(n + 1));
}
