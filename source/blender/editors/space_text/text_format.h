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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_text/text_format.h
 *  \ingroup sptext
 */

#ifndef __TEXT_FORMAT_H__
#define __TEXT_FORMAT_H__

/* *** Flatten String *** */
typedef struct FlattenString {
	char fixedbuf[256];
	int fixedaccum[256];

	char *buf;
	int *accum;
	int pos, len;
} FlattenString;

int  flatten_string(struct SpaceText *st, FlattenString *fs, const char *in);
void flatten_string_free(FlattenString *fs);
int  flatten_string_strlen(FlattenString *fs, const char *str);

int  text_check_format_len(TextLine *line, unsigned int len);


/* *** Generalize Formatting *** */
typedef struct TextFormatType {
	struct TextFormatType *next, *prev;

	/* Formats the specified line. If do_next is set, the process will move on to
	 * the succeeding line if it is affected (eg. multiline strings). Format strings
	 * may contain any of the following characters:
	 *  '_'  Whitespace
	 *  '#'  Comment text
	 *  '!'  Punctuation and other symbols
	 *  'n'  Numerals
	 *  'l'  String letters
	 *  'v'  Special variables (class, def)
	 *  'b'  Built-in names (print, for, etc.)
	 *  'q'  Other text (identifiers, etc.)
	 * It is terminated with a null-terminator '\0' followed by a continuation
	 * flag indicating whether the line is part of a multi-line string. */
	void (*format_line)(SpaceText *st, TextLine *line, int do_next);

	const char **ext;  /* NULL terminated extensions */
} TextFormatType;

TextFormatType *ED_text_format_get(Text *text);
void            ED_text_format_register(TextFormatType *tft);

/* formatters */
void ED_text_format_register_py(void);

#endif  /* __TEXT_FORMAT_H__ */
