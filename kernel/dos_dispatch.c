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
