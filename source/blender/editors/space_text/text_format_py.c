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

/** \file blender/editors/space_text/text_format_py.c
 *  \ingroup sptext
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "MEM_guardedalloc.h"

#include "BLF_api.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_text_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_suggestions.h"
#include "BKE_text.h"

#include "BIF_gl.h"

#include "ED_datafiles.h"
#include "UI_interface.h"
#include "UI_resources.h"

#include "text_intern.h"
#include "text_format.h"

/* *** Local Functions (for format_line) *** */


/* Checks the specified source string for a Python built-in function name. This
 * name must start at the beginning of the source string and must be followed by
 * a non-identifier (see text_check_identifier(char)) or null character.
 *
 * If a built-in function is found, the length of the matching name is returned.
 * Otherwise, -1 is returned.
 *
 * See:
 * http://docs.python.org/py3k/reference/lexical_analysis.html#keywords
 */

static int find_builtinfunc(char *string)
{
	int a, i;
	const char *builtinfuncs[] = {
		/* "False", "None", "True", */ /* see find_bool() */
		"and", "as", "assert", "break",
		"class", "continue", "def", "del", "elif", "else", "except",
		"finally", "for", "from", "global", "if", "import", "in",
		"is", "lambda", "nonlocal", "not", "or", "pass", "raise",
		"return", "try", "while", "with", "yield",
	};

	for (a = 0; a < sizeof(builtinfuncs) / sizeof(builtinfuncs[0]); a++) {
		i = 0;
		while (1) {
			/* If we hit the end of a keyword... (eg. "def") */
			if (builtinfuncs[a][i] == '\0') {
				/* If we still have identifier chars in the source (eg. "definate") */
				if (text_check_identifier(string[i]))
					i = -1;  /* No match */
				break; /* Next keyword if no match, otherwise we're done */

				/* If chars mismatch, move on to next keyword */
			}
			else if (string[i] != builtinfuncs[a][i]) {
				i = -1;
				break; /* Break inner loop, start next keyword */
			}
			i++;
		}
		if (i > 0) break;  /* If we have a match, we're done */
	}
	return i;
}

/* Checks the specified source string for a Python special name. This name must
 * start at the beginning of the source string and must be followed by a non-
 * identifier (see text_check_identifier(char)) or null character.
 *
 * If a special name is found, the length of the matching name is returned.
 * Otherwise, -1 is returned. */

static int find_specialvar(char *string)
{
	int i = 0;
	/* Check for "def" */
	if (string[0] == 'd' && string[1] == 'e' && string[2] == 'f')
		i = 3;
	/* Check for "class" */
	else if (string[0] == 'c' && string[1] == 'l' && string[2] == 'a' && string[3] == 's' && string[4] == 's')
		i = 5;
	/* If next source char is an identifier (eg. 'i' in "definate") no match */
	if (i == 0 || text_check_identifier(string[i]))
		return -1;
	return i;
}

static int find_decorator(char *string)
{
	if (string[0] == '@') {
		int i = 1;
		while (text_check_identifier(string[i])) {
			i++;
		}
		return i;
	}
	return -1;
}

static int find_bool(char *string)
{
	int i = 0;
	/* Check for "False" */
	if (string[0] == 'F' && string[1] == 'a' && string[2] == 'l' && string[3] == 's' && string[4] == 'e')
		i = 5;
	/* Check for "True" */
	else if (string[0] == 'T' && string[1] == 'r' && string[2] == 'u' && string[3] == 'e')
		i = 4;
	/* Check for "None" */
	else if (string[0] == 'N' && string[1] == 'o' && string[2] == 'n' && string[3] == 'e')
		i = 4;
	/* If next source char is an identifier (eg. 'i' in "definate") no match */
	if (i == 0 || text_check_identifier(string[i]))
		return -1;
	return i;
}

static void txt_format_line(SpaceText *st, TextLine *line, int do_next)
{
	FlattenString fs;
	char *str, *fmt, orig, cont, find, prev = ' ';
	int len, i;

	/* Get continuation from previous line */
	if (line->prev && line->prev->format != NULL) {
		fmt = line->prev->format;
		cont = fmt[strlen(fmt) + 1]; /* Just after the null-terminator */
	}
	else cont = 0;

	/* Get original continuation from this line */
	if (line->format != NULL) {
		fmt = line->format;
		orig = fmt[strlen(fmt) + 1]; /* Just after the null-terminator */
	}
	else orig = 0xFF;

	len = flatten_string(st, &fs, line->line);
	str = fs.buf;
	if (!text_check_format_len(line, len)) {
		flatten_string_free(&fs);
		return;
	}
	fmt = line->format;

	while (*str) {
		/* Handle escape sequences by skipping both \ and next char */
		if (*str == '\\') {
			*fmt = prev; fmt++; str++;
			if (*str == '\0') break;
			*fmt = prev; fmt++; str += BLI_str_utf8_size_safe(str);
			continue;
		}
		/* Handle continuations */
		else if (cont) {
			/* Triple strings ("""...""" or '''...''') */
			if (cont & TXT_TRISTR) {
				find = (cont & TXT_DBLQUOTSTR) ? '"' : '\'';
				if (*str == find && *(str + 1) == find && *(str + 2) == find) {
					*fmt = 'l'; fmt++; str++;
					*fmt = 'l'; fmt++; str++;
					cont = 0;
				}
				/* Handle other strings */
			}
			else {
				find = (cont & TXT_DBLQUOTSTR) ? '"' : '\'';
				if (*str == find) cont = 0;
			}

			*fmt = 'l';
			str += BLI_str_utf8_size_safe(str) - 1;
		}
		/* Not in a string... */
		else {
			/* Deal with comments first */
			if (prev == '#' || *str == '#') {
				*fmt = '#';
				str += BLI_str_utf8_size_safe(str) - 1;
			}
			else if (*str == '"' || *str == '\'') {
				/* Strings */
				find = *str;
				cont = (*str == '"') ? TXT_DBLQUOTSTR : TXT_SNGQUOTSTR;
				if (*(str + 1) == find && *(str + 2) == find) {
					*fmt = 'l'; fmt++; str++;
					*fmt = 'l'; fmt++; str++;
					cont |= TXT_TRISTR;
				}
				*fmt = 'l';
			}
			/* Whitespace (all ws. has been converted to spaces) */
			else if (*str == ' ')
				*fmt = '_';
			/* Numbers (digits not part of an identifier and periods followed by digits) */
			else if ((prev != 'q' && text_check_digit(*str)) || (*str == '.' && text_check_digit(*(str + 1))))
				*fmt = 'n';
			/* Booleans */
			else if (prev != 'q' && (i = find_bool(str)) != -1)
				if (i > 0) {
					while (i > 1) {
						*fmt = 'n'; fmt++; str++;
						i--;
					}
					*fmt = 'n';
				}
				else {
					str += BLI_str_utf8_size_safe(str) - 1;
					*fmt = 'q';
				}
			/* Punctuation */
			else if (text_check_delim(*str))
				*fmt = '!';
			/* Identifiers and other text (no previous ws. or delims. so text continues) */
			else if (prev == 'q') {
				str += BLI_str_utf8_size_safe(str) - 1;
				*fmt = 'q';
			}
			/* Not ws, a digit, punct, or continuing text. Must be new, check for special words */
			else {
				/* Special vars(v) or built-in keywords(b) */
				if ((i = find_specialvar(str)) != -1)
					prev = 'v';
				else if ((i = find_builtinfunc(str)) != -1)
					prev = 'b';
				else if ((i = find_decorator(str)) != -1)
					prev = 'v';  /* could have a new color for this */
				if (i > 0) {
					while (i > 1) {
						*fmt = prev; fmt++; str++;
						i--;
					}
					*fmt = prev;
				}
				else {
					str += BLI_str_utf8_size_safe(str) - 1;
					*fmt = 'q';
				}
			}
		}
		prev = *fmt;
		fmt++;
		str++;
	}

	/* Terminate and add continuation char */
	*fmt = '\0'; fmt++;
	*fmt = cont;

	/* If continuation has changed and we're allowed, process the next line */
	if (cont != orig && do_next && line->next) {
		txt_format_line(st, line->next, do_next);
	}

	flatten_string_free(&fs);
}

void ED_text_format_register_py(void)
{
	static TextFormatType tft = {0};
	static const char *ext[] = {"py", NULL};

	tft.format_line = txt_format_line;
	tft.ext = ext;

	ED_text_format_register(&tft);
}
