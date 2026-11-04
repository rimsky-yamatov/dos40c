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

/* ---- 3Dh/3Eh/3Fh/40h/42h: handle-based file I/O ---- */
static void fn_open(struct regs *r)
{
	char path[80];
	int h;

	get_path(r, path, sizeof(path));
	h = h_open(path, AL(r));
	if (h < 0)
		ret_err(r, -h);
	else {
		r->ax = (u16)h;
		ret_ok(r);
	}
}

static void fn_close(struct regs *r)
{
	int e = h_close((int)r->bx);

	if (e < 0)
		ret_err(r, -e);
	else
		ret_ok(r);
}

static void fn_read(struct regs *r)
{
	int h = (int)r->bx, type = h_type(h);
	u16 want = r->cx, got = 0, off = r->dx;
	u8 buf[512];

	if (type < 0) {
		ret_err(r, -type);
		return;
	}
	if (type == H_TYPE_NUL) {
		r->ax = 0;
		ret_ok(r);
		return;
	}
	if (type == H_TYPE_CON) {	/* stdin: one edited line */
		char line[256];
		int n = con_readline(line, 255);

		line[n] = '\r';
		line[n + 1] = '\n';
		n += 2;
		if (n > (int)want)
			n = (int)want;
		far_copy_to(r->ds, off, line, (u16)n);
		r->ax = (u16)n;
		ret_ok(r);
		return;
	}
	while (got < want) {
		u16 chunk = (u16)(want - got > 512 ? 512 : want - got);
		int n = h_read(h, buf, chunk);

		if (n < 0) {
			ret_err(r, -n);
			return;
		}
		if (n == 0)
			break;
		far_copy_to(r->ds, (u16)(off + got), buf, (u16)n);
		got = (u16)(got + n);
	}
	r->ax = got;
	ret_ok(r);
}

static void fn_write(struct regs *r)
{
	int type = h_type((int)r->bx);
	u16 i;

	if (type < 0) {
		ret_err(r, -type);
		return;
	}
	if (type == H_TYPE_NUL) {
		r->ax = r->cx;
		ret_ok(r);
		return;
	}
	if (type != H_TYPE_CON || r->bx == 0) {	/* no writable disk files */
		ret_err(r, ERR_ACCESS_DENIED);
		return;
	}
	for (i = 0; i < r->cx; i++)
		con_putc((char)far_rd8(r->ds, (u16)(r->dx + i)));
	r->ax = r->cx;
	ret_ok(r);
}

static void fn_seek(struct regs *r)
{
	i32 off = (i32)(((u32)r->cx << 16) | r->dx);
	i32 np = h_seek((int)r->bx, off, AL(r));

	if (np < 0) {
		ret_err(r, (int)-np);
		return;
	}
	r->dx = (u16)((u32)np >> 16);
	r->ax = (u16)np;
	ret_ok(r);
}
