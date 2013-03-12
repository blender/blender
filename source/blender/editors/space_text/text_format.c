/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_text/text_format.c
 *  \ingroup sptext
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "DNA_text_types.h"
#include "DNA_space_types.h"

#include "text_format.h"


/****************** flatten string **********************/

static void flatten_string_append(FlattenString *fs, const char *c, int accum, int len)
{
	int i;

	if (fs->pos + len > fs->len) {
		char *nbuf; int *naccum;
		fs->len *= 2;

		nbuf = MEM_callocN(sizeof(*fs->buf) * fs->len, "fs->buf");
		naccum = MEM_callocN(sizeof(*fs->accum) * fs->len, "fs->accum");

		memcpy(nbuf, fs->buf, fs->pos * sizeof(*fs->buf));
		memcpy(naccum, fs->accum, fs->pos * sizeof(*fs->accum));

		if (fs->buf != fs->fixedbuf) {
			MEM_freeN(fs->buf);
			MEM_freeN(fs->accum);
		}

		fs->buf = nbuf;
		fs->accum = naccum;
	}

	for (i = 0; i < len; i++) {
		fs->buf[fs->pos + i] = c[i];
		fs->accum[fs->pos + i] = accum;
	}

	fs->pos += len;
}

int flatten_string(SpaceText *st, FlattenString *fs, const char *in)
{
	int r, i, total = 0;

	memset(fs, 0, sizeof(FlattenString));
	fs->buf = fs->fixedbuf;
	fs->accum = fs->fixedaccum;
	fs->len = sizeof(fs->fixedbuf);

	for (r = 0, i = 0; *in; r++) {
		if (*in == '\t') {
			i = st->tabnumber - (total % st->tabnumber);
			total += i;

			while (i--)
				flatten_string_append(fs, " ", r, 1);

			in++;
		}
		else {
			size_t len = BLI_str_utf8_size_safe(in);
			flatten_string_append(fs, in, r, len);
			in += len;
			total++;
		}
	}

	flatten_string_append(fs, "\0", r, 1);

	return total;
}

void flatten_string_free(FlattenString *fs)
{
	if (fs->buf != fs->fixedbuf)
		MEM_freeN(fs->buf);
	if (fs->accum != fs->fixedaccum)
		MEM_freeN(fs->accum);
}

/* takes a string within fs->buf and returns its length */
int flatten_string_strlen(FlattenString *fs, const char *str)
{
	const int len = (fs->pos - (int)(str - fs->buf)) - 1;
	BLI_assert(strlen(str) == len);
	return len;
}

/* Ensures the format string for the given line is long enough, reallocating
 * as needed. Allocation is done here, alone, to ensure consistency. */
int text_check_format_len(TextLine *line, unsigned int len)
{
	if (line->format) {
		if (strlen(line->format) < len) {
			MEM_freeN(line->format);
			line->format = MEM_mallocN(len + 2, "SyntaxFormat");
			if (!line->format) return 0;
		}
	}
	else {
		line->format = MEM_mallocN(len + 2, "SyntaxFormat");
		if (!line->format) return 0;
	}

	return 1;
}

/**
 * Fill the string with formatting constant,
 * advancing \a str_p and \a fmt_p
 *
 * \param len length in bytes of \a fmt_p to fill.
 */
void text_format_fill(const char **str_p, char **fmt_p, const char type, const int len)
{
	const char *str = *str_p;
	char *fmt = *fmt_p;
	int i = 0;

	while (i < len) {
		const int size = BLI_str_utf8_size_safe(str);
		*fmt++ = type;

		str += size;
		i   += 1;
	}

	str--;
	fmt--;

	BLI_assert(*str != '\0');

	*str_p = str;
	*fmt_p = fmt;
}
/**
 * ascii version of #text_format_fill,
 * use when we no the text being stepped over is ascii (as is the case for most keywords)
 */
void text_format_fill_ascii(const char **str_p, char **fmt_p, const char type, const int len)
{
	const char *str = *str_p;
	char *fmt = *fmt_p;

	memset(fmt, type, len);

	str += len - 1;
	fmt += len - 1;

	BLI_assert(*str != '\0');

	*str_p = str;
	*fmt_p = fmt;
}

/* *** Registration *** */
static ListBase tft_lb = {NULL, NULL};
void ED_text_format_register(TextFormatType *tft)
{
	BLI_addtail(&tft_lb, tft);
}

TextFormatType *ED_text_format_get(Text *text)
{
	TextFormatType *tft;

	if (text) {
		const char *text_ext = strchr(text->id.name + 2, '.');
		if (text_ext) {
			text_ext++;  /* skip the '.' */
			/* Check all text formats in the static list */
			for (tft = tft_lb.first; tft; tft = tft->next) {
				/* All formats should have an ext, but just in case */
				const char **ext;
				for (ext = tft->ext; *ext; ext++) {
					/* If extension matches text name, return the matching tft */
					if (BLI_strcasecmp(text_ext, *ext) == 0) {
						return tft;
					}
				}
			}
		}

		/* If we make it here we never found an extension that worked - return 
		 * the "default" text format */
		return tft_lb.first;
	}
	else {
		/* Return the "default" text format */
		return tft_lb.first;
	}
}
