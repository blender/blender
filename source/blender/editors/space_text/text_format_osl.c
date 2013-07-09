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

/** \file blender/editors/space_text/text_format_osl.c
 *  \ingroup sptext
 */

#include <string.h>

#include "BLI_blenlib.h"

#include "DNA_text_types.h"
#include "DNA_space_types.h"

#include "BKE_text.h"

#include "text_format.h"

/* *** Local Functions (for format_line) *** */

static int txtfmt_osl_find_builtinfunc(const char *string)
{
	int i, len;
	/* list is from
	 * https://github.com/imageworks/OpenShadingLanguage/raw/master/src/doc/osl-languagespec.pdf
	 */
	if      (STR_LITERAL_STARTSWITH(string, "break",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "closure",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "color",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "continue",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "do",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "else",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "emit",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "float",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "for",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "if",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "illuminance",  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "illuminate",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "int",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "matrix",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "normal",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "output",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "point",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "public",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "return",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "string",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "struct",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "vector",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "void",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "while",        len)) i = len;
	else                                                          i = 0;

	/* If next source char is an identifier (eg. 'i' in "definate") no match */
	if (i == 0 || text_check_identifier(string[i]))
		return -1;
	return i;
}

static int txtfmt_osl_find_reserved(const char *string)
{
	int i, len;
	/* list is from...
	 * https://github.com/imageworks/OpenShadingLanguage/raw/master/src/doc/osl-languagespec.pdf
	 */
	if      (STR_LITERAL_STARTSWITH(string, "bool",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "case",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "catch",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "char",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "const",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "delete",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "default",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "double",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "enum",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "extern",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "false",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "friend",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "goto",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "inline",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "long",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "new",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "operator",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "private",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "protected",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "short",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "signed",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sizeof",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "static",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "switch",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "template",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "this",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "throw",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "true",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "try",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "typedef",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "uniform",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "union",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "unsigned",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "varying",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "virtual",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "volatile",     len)) i = len;
	else                                                          i = 0;

	/* If next source char is an identifier (eg. 'i' in "definate") no match */
	if (i == 0 || text_check_identifier(string[i]))
		return -1;
	return i;
}

/* Checks the specified source string for a OSL special name. This name must
 * start at the beginning of the source string and must be followed by a non-
 * identifier (see text_check_identifier(char)) or null character.
 *
 * If a special name is found, the length of the matching name is returned.
 * Otherwise, -1 is returned. */

static int txtfmt_osl_find_specialvar(const char *string)
{
	int i, len;
	
	/* OSL shader types */
	if      (STR_LITERAL_STARTSWITH(string, "shader",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "surface",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "volume",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "displacement", len)) i = len;
	else                                                    i = 0;

	/* If next source char is an identifier (eg. 'i' in "definate") no match */
	if (i == 0 || text_check_identifier(string[i]))
		return -1;
	return i;
}

/* matches py 'txtfmt_osl_find_decorator' */
static int txtfmt_osl_find_preprocessor(const char *string)
{
	if (string[0] == '#') {
		int i = 1;
		/* Whitespace is ok '#  foo' */
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

static char txtfmt_osl_format_identifier(const char *str)
{
	char fmt;
	if      ((txtfmt_osl_find_specialvar(str))   != -1) fmt = FMT_TYPE_SPECIAL;
	else if ((txtfmt_osl_find_builtinfunc(str))  != -1) fmt = FMT_TYPE_KEYWORD;
	else if ((txtfmt_osl_find_reserved(str))     != -1) fmt = FMT_TYPE_RESERVED;
	else if ((txtfmt_osl_find_preprocessor(str)) != -1) fmt = FMT_TYPE_DIRECTIVE;
	else                                                fmt = FMT_TYPE_DEFAULT;
	return fmt;
}

static void txtfmt_osl_format_line(SpaceText *st, TextLine *line, const int do_next)
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
			/* C-Style comments */
			if (cont & FMT_CONT_COMMENT_C) {
				if (*str == '*' && *(str + 1) == '/') {
					*fmt = FMT_TYPE_COMMENT; fmt++; str++;
					*fmt = FMT_TYPE_COMMENT;
					cont = FMT_CONT_NOP;
				}
				else {
					*fmt = FMT_TYPE_COMMENT;
				}
				/* Handle other comments */
			}
			else {
				find = (cont & FMT_CONT_QUOTEDOUBLE) ? '"' : '\'';
				if (*str == find) cont = 0;
				*fmt = FMT_TYPE_STRING;
			}

			str += BLI_str_utf8_size_safe(str) - 1;
		}
		/* Not in a string... */
		else {
			/* Deal with comments first */
			if (*str == '/' && *(str + 1) == '/') {
				/* fill the remaining line */
				text_format_fill(&str, &fmt, FMT_TYPE_COMMENT, len - (int)(fmt - line->format));
			}
			/* C-Style (multi-line) comments */
			else if (*str == '/' && *(str + 1) == '*') {
				cont = FMT_CONT_COMMENT_C;
				*fmt = FMT_TYPE_COMMENT; fmt++; str++;
				*fmt = FMT_TYPE_COMMENT;
			}
			else if (*str == '"' || *str == '\'') {
				/* Strings */
				find = *str;
				cont = (*str == '"') ? FMT_CONT_QUOTEDOUBLE : FMT_CONT_QUOTESINGLE;
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
			/* Punctuation */
			else if ((*str != '#') && text_check_delim(*str)) {
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
				/* keep in sync with 'txtfmt_osl_format_identifier()' */
				if      ((i = txtfmt_osl_find_specialvar(str))   != -1) prev = FMT_TYPE_SPECIAL;
				else if ((i = txtfmt_osl_find_builtinfunc(str))  != -1) prev = FMT_TYPE_KEYWORD;
				else if ((i = txtfmt_osl_find_reserved(str))     != -1) prev = FMT_TYPE_RESERVED;
				else if ((i = txtfmt_osl_find_preprocessor(str)) != -1) prev = FMT_TYPE_DIRECTIVE;

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
		txtfmt_osl_format_line(st, line->next, do_next);
	}

	flatten_string_free(&fs);
}

void ED_text_format_register_osl(void)
{
	static TextFormatType tft = {NULL};
	static const char *ext[] = {"osl", NULL};

	tft.format_identifier = txtfmt_osl_format_identifier;
	tft.format_line       = txtfmt_osl_format_line;
	tft.ext = ext;

	ED_text_format_register(&tft);
}
