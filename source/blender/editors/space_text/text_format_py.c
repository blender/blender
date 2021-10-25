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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_text/text_format_py.c
 *  \ingroup sptext
 */

#include <string.h>

#include "BLI_blenlib.h"

#include "DNA_text_types.h"
#include "DNA_space_types.h"

#include "BKE_text.h"

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

static int txtfmt_py_find_builtinfunc(const char *string)
{
	int i, len;
	/* list is from...
	 * ", ".join(['"%s"' % kw
	 *            for kw in  __import__("keyword").kwlist
	 *            if kw not in {"False", "None", "True", "def", "class"}])
	 *
	 * ... and for this code:
	 * print("\n".join(['else if (STR_LITERAL_STARTSWITH(string, "%s", len)) i = len;' % kw
	 *                  for kw in  __import__("keyword").kwlist
	 *                  if kw not in {"False", "None", "True", "def", "class"}]))
	 */

	if      (STR_LITERAL_STARTSWITH(string, "and",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "as",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "assert",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "break",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "continue", len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "del",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "elif",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "else",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "except",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "finally",  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "for",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "from",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "global",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "if",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "import",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "in",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "is",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "lambda",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "nonlocal", len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "not",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "or",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pass",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "raise",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "return",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "try",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "while",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "with",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "yield",    len)) i = len;
	else                                                      i = 0;

	/* If next source char is an identifier (eg. 'i' in "definate") no match */
	if (i == 0 || text_check_identifier(string[i]))
		return -1;
	return i;
}

/* Checks the specified source string for a Python special name. This name must
 * start at the beginning of the source string and must be followed by a non-
 * identifier (see text_check_identifier(char)) or null character.
 *
 * If a special name is found, the length of the matching name is returned.
 * Otherwise, -1 is returned. */

static int txtfmt_py_find_specialvar(const char *string)
{
	int i, len;

	if      (STR_LITERAL_STARTSWITH(string, "def", len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "class", len)) i = len;
	else                                                   i = 0;

	/* If next source char is an identifier (eg. 'i' in "definate") no match */
	if (i == 0 || text_check_identifier(string[i]))
		return -1;
	return i;
}

static int txtfmt_py_find_decorator(const char *string)
{
	if (string[0] == '@') {
		int i = 1;
		/* Whitespace is ok '@  foo' */
		while (text_check_whitespace(string[i])) {
			i++;
		}
		while (text_check_identifier(string[i])) {
			i++;
		}
		return i;
	}
	return -1;
}

static int txtfmt_py_find_bool(const char *string)
{
	int i, len;

	if      (STR_LITERAL_STARTSWITH(string, "None",  len))  i = len;
	else if (STR_LITERAL_STARTSWITH(string, "True",  len))  i = len;
	else if (STR_LITERAL_STARTSWITH(string, "False", len))  i = len;
	else                                                    i = 0;

	/* If next source char is an identifier (eg. 'i' in "Nonetheless") no match */
	if (i == 0 || text_check_identifier(string[i]))
		return -1;
	return i;
}

static char txtfmt_py_format_identifier(const char *str)
{
	char fmt;
	if      ((txtfmt_py_find_specialvar(str))   != -1) fmt = FMT_TYPE_SPECIAL;
	else if ((txtfmt_py_find_builtinfunc(str))  != -1) fmt = FMT_TYPE_KEYWORD;
	else if ((txtfmt_py_find_decorator(str))    != -1) fmt = FMT_TYPE_RESERVED;
	else                                               fmt = FMT_TYPE_DEFAULT;
	return fmt;
}

static void txtfmt_py_format_line(SpaceText *st, TextLine *line, const bool do_next)
{
	FlattenString fs;
	const char *str;
	char *fmt;
	char cont_orig, cont, find, prev = ' ';
	int len, i;

	/* Get continuation from previous line */
	if (line->prev && line->prev->format != NULL) {
		fmt = line->prev->format;
		cont = fmt[strlen(fmt) + 1]; /* Just after the null-terminator */
		BLI_assert((FMT_CONT_ALL & cont) == cont);
	}
	else {
		cont = FMT_CONT_NOP;
	}

	/* Get original continuation from this line */
	if (line->format != NULL) {
		fmt = line->format;
		cont_orig = fmt[strlen(fmt) + 1]; /* Just after the null-terminator */
		BLI_assert((FMT_CONT_ALL & cont_orig) == cont_orig);
	}
	else {
		cont_orig = 0xFF;
	}

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
			if (cont & FMT_CONT_TRIPLE) {
				find = (cont & FMT_CONT_QUOTEDOUBLE) ? '"' : '\'';
				if (*str == find && *(str + 1) == find && *(str + 2) == find) {
					*fmt = FMT_TYPE_STRING; fmt++; str++;
					*fmt = FMT_TYPE_STRING; fmt++; str++;
					cont = FMT_CONT_NOP;
				}
				/* Handle other strings */
			}
			else {
				find = (cont & FMT_CONT_QUOTEDOUBLE) ? '"' : '\'';
				if (*str == find) cont = FMT_CONT_NOP;
			}

			*fmt = FMT_TYPE_STRING;
			str += BLI_str_utf8_size_safe(str) - 1;
		}
		/* Not in a string... */
		else {
			/* Deal with comments first */
			if (*str == '#') {
				/* fill the remaining line */
				text_format_fill(&str, &fmt, FMT_TYPE_COMMENT, len - (int)(fmt - line->format));
			}
			else if (*str == '"' || *str == '\'') {
				/* Strings */
				find = *str;
				cont = (*str == '"') ? FMT_CONT_QUOTEDOUBLE : FMT_CONT_QUOTESINGLE;
				if (*(str + 1) == find && *(str + 2) == find) {
					*fmt = FMT_TYPE_STRING; fmt++; str++;
					*fmt = FMT_TYPE_STRING; fmt++; str++;
					cont |= FMT_CONT_TRIPLE;
				}
				*fmt = FMT_TYPE_STRING;
			}
			/* Whitespace (all ws. has been converted to spaces) */
			else if (*str == ' ') {
				*fmt = FMT_TYPE_WHITESPACE;
			}
			/* Numbers (digits not part of an identifier and periods followed by digits) */
			else if ((prev != FMT_TYPE_DEFAULT && text_check_digit(*str)) ||
			         (*str == '.' && text_check_digit(*(str + 1))))
			{
				*fmt = FMT_TYPE_NUMERAL;
			}
			/* Booleans */
			else if (prev != FMT_TYPE_DEFAULT && (i = txtfmt_py_find_bool(str)) != -1) {
				if (i > 0) {
					text_format_fill_ascii(&str, &fmt, FMT_TYPE_NUMERAL, i);
				}
				else {
					str += BLI_str_utf8_size_safe(str) - 1;
					*fmt = FMT_TYPE_DEFAULT;
				}
			}
			/* Punctuation */
			else if ((*str != '@') && text_check_delim(*str)) {
				*fmt = FMT_TYPE_SYMBOL;
			}
			/* Identifiers and other text (no previous ws. or delims. so text continues) */
			else if (prev == FMT_TYPE_DEFAULT) {
				str += BLI_str_utf8_size_safe(str) - 1;
				*fmt = FMT_TYPE_DEFAULT;
			}
			/* Not ws, a digit, punct, or continuing text. Must be new, check for special words */
			else {
				/* Special vars(v) or built-in keywords(b) */
				/* keep in sync with 'txtfmt_py_format_identifier()' */
				if      ((i = txtfmt_py_find_specialvar(str))   != -1) prev = FMT_TYPE_SPECIAL;
				else if ((i = txtfmt_py_find_builtinfunc(str))  != -1) prev = FMT_TYPE_KEYWORD;
				else if ((i = txtfmt_py_find_decorator(str))    != -1) prev = FMT_TYPE_DIRECTIVE;

				if (i > 0) {
					if (prev == FMT_TYPE_DIRECTIVE) {  /* can contain utf8 */
						text_format_fill(&str, &fmt, prev, i);
					}
					else {
						text_format_fill_ascii(&str, &fmt, prev, i);
					}
				}
				else {
					str += BLI_str_utf8_size_safe(str) - 1;
					*fmt = FMT_TYPE_DEFAULT;
				}
			}
		}
		prev = *fmt; fmt++; str++;
	}

	/* Terminate and add continuation char */
	*fmt = '\0'; fmt++;
	*fmt = cont;

	/* If continuation has changed and we're allowed, process the next line */
	if (cont != cont_orig && do_next && line->next) {
		txtfmt_py_format_line(st, line->next, do_next);
	}

	flatten_string_free(&fs);
}

void ED_text_format_register_py(void)
{
	static TextFormatType tft = {NULL};
	static const char *ext[] = {"py", NULL};

	tft.format_identifier = txtfmt_py_format_identifier;
	tft.format_line       = txtfmt_py_format_line;
	tft.ext = ext;

	ED_text_format_register(&tft);
}
