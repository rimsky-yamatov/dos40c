/* dos_dir.c - root directory and path parsing, ported from DOS/DIR.ASM,
 * PATH.ASM and SEARCH.ASM.  Only the root directory of drive A: exists. */
#include "dos.h"
#include "lib.h"

static u16 rd16(const u8 *p)
{
	return (u16)(p[0] | (p[1] << 8));
}

static u32 rd32(const u8 *p)
{
	return (u32)rd16(p) | ((u32)rd16(p + 2) << 16);
}

/* Read directory slot `index`. */
int dir_get(u16 index, struct dirent *de)
{
	const u8 *s, *e;

	if (index >= g_vol.rootents)
		return DIR_END;
	s = fat_sector(g_vol.root_lba + index / 16);
	if (!s)
		return DIR_ERROR;
	e = s + (index % 16) * 32;
	if (e[0] == 0x00)
		return DIR_END;
	if (e[0] == 0xE5 || (e[11] & 0x0F) == 0x0F)
		return DIR_SKIP;
	memcpy(de->name, e, 11);
	de->attr    = e[11];
	de->time    = rd16(e + 22);
	de->date    = rd16(e + 24);
	de->cluster = rd16(e + 26);
	de->size    = rd32(e + 28);
	return DIR_OK;
}

static char upcase(char c)
{
	return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

/* Parse one name component ("NAME" or "EXT") into a space padded field.
 * '*' fills the rest of the field with '?'.  Returns 0 / -ERR_*. */
static int parse_field(const char **pp, u8 *out, int max)
{
	const char *p = *pp;
	int i = 0, star = 0;

	while (*p && *p != '.' && *p != '\\' && *p != '/') {
		char c = *p++;

		if (star)
			continue;
		if (c == '*') {
			while (i < max)
				out[i++] = '?';
			star = 1;
			continue;
		}
		if (i >= max)
			return -ERR_FILE_NOT_FOUND;
		out[i++] = (u8)upcase(c);
	}
	*pp = p;
	return 0;
}

/* "A:\NAME.EXT" -> 11 byte FCB name.  Sub-directories are not supported. */
int path_to_fcb(const char *p, u8 fcb[11])
{
	int r;

	memset(fcb, ' ', 11);
	if (p[0] && p[1] == ':') {
		if (upcase(p[0]) != 'A')
			return -ERR_INVALID_DRIVE;
		p += 2;
	}
	if (*p == '\\' || *p == '/')
		p++;
	if (*p == 0)
		return -ERR_FILE_NOT_FOUND;
	r = parse_field(&p, fcb, 8);
	if (r)
		return r;
	if (*p == '.') {
		p++;
		r = parse_field(&p, fcb + 8, 3);
		if (r)
			return r;
	}
	if (*p == '\\' || *p == '/')
		return -ERR_PATH_NOT_FOUND;
	if (*p == '.')
		return -ERR_FILE_NOT_FOUND;
	return 0;
}

static int fcb_match(const u8 *pat, const u8 *name)
{
	int i;

	for (i = 0; i < 11; i++)
		if (pat[i] != '?' && pat[i] != name[i])
			return 0;
	return 1;
}

/* exact lookup (no volume labels) */
int dir_find(const u8 fcb[11], struct dirent *out)
{
	u16 i;

	for (i = 0; ; i++) {
		int r = dir_get(i, out);

		if (r == DIR_END)
			break;
		if (r == DIR_ERROR)
			return -ERR_READ_FAULT;
		if (r == DIR_OK && !(out->attr & ATTR_VOLUME) &&
		    fcb_match(fcb, out->name))
			return 0;
	}
	return -ERR_FILE_NOT_FOUND;
}

/* FindFirst/FindNext engine: continue at *index, honour attribute mask */
int dir_search(const u8 pattern[11], u8 attrmask, u16 *index,
	       struct dirent *out)
{
	u16 i;

	for (i = *index; ; i++) {
		int r = dir_get(i, out);

		if (r == DIR_END)
			break;
		if (r == DIR_ERROR)
			return -ERR_READ_FAULT;
		if (r != DIR_OK)
			continue;
		if (out->attr & ATTR_VOLUME) {
			if (!(attrmask & ATTR_VOLUME))
				continue;
		} else if ((out->attr & (ATTR_HIDDEN | ATTR_SYSTEM | ATTR_DIR))
			   & ~attrmask) {
			continue;
		}
		if (fcb_match(pattern, out->name)) {
			*index = (u16)(i + 1);
			return 0;
		}
	}
	return -ERR_NO_MORE_FILES;
}

/* "NAME    EXT" -> "NAME.EXT" */
void fcb_to_name(const u8 fcb[11], char out[13])
{
	int i, n = 0;

	for (i = 0; i < 8 && fcb[i] != ' '; i++)
		out[n++] = (char)fcb[i];
	if (fcb[8] != ' ') {
		out[n++] = '.';
		for (i = 8; i < 11 && fcb[i] != ' '; i++)
			out[n++] = (char)fcb[i];
	}
	out[n] = 0;
}
