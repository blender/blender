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

/** \file blender/editors/space_text/text_format_lua.c
 *  \ingroup sptext
 */

#include <string.h>

#include "BLI_blenlib.h"

#include "DNA_text_types.h"
#include "DNA_space_types.h"

#include "BKE_text.h"

#include "text_format.h"

/* *** Lua Keywords (for format_line) *** */

/* Checks the specified source string for a Lua keyword (minus boolean & 'nil'). 
 * This name must start at the beginning of the source string and must be 
 * followed by a non-identifier (see text_check_identifier(char)) or null char.
 *
 * If a keyword is found, the length of the matching word is returned.
 * Otherwise, -1 is returned.
 *
 * See:
 * http://www.lua.org/manual/5.1/manual.html#2.1
 */

static int txtfmt_lua_find_keyword(const char *string)
{
	int i, len;

	if      (STR_LITERAL_STARTSWITH(string, "and",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "break",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "do",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "else",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "elseif",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "end",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "for",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "function", len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "if",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "in",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "local",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "not",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "or",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "repeat",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "return",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "then",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "until",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "while",    len)) i = len;
	else                                                      i = 0;

	/* If next source char is an identifier (eg. 'i' in "definate") no match */
	if (i == 0 || text_check_identifier(string[i]))
		return -1;
	return i;
}

/* Checks the specified source string for a Lua special name/function. This 
 * name must start at the beginning of the source string and must be followed 
 * by a non-identifier (see text_check_identifier(char)) or null character.
 *
 * If a special name is found, the length of the matching name is returned.
 * Otherwise, -1 is returned. 
 * 
 * See:
 * http://www.lua.org/manual/5.1/manual.html#5.1
 */

static int txtfmt_lua_find_specialvar(const char *string)
{
	int i, len;

	if      (STR_LITERAL_STARTSWITH(string, "assert",           len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "collectgarbage",   len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "dofile",           len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "error",            len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "_G",               len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "getfenv",          len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "getmetatable",     len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "__index",          len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ipairs",           len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "load",             len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "loadfile",         len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "loadstring",       len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "next",             len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pairs",            len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pcall",            len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "print",            len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "rawequal",         len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "rawget",           len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "rawset",           len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "select",           len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "setfenv",          len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "setmetatable",     len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tonumber",         len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tostring",         len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "type",             len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "unpack",           len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "_VERSION",         len))   i = len;
	else if (STR_LITERAL_STARTSWITH(string, "xpcall",           len))   i = len;
	else                                                i = 0;

	/* If next source char is an identifier (eg. 'i' in "definate") no match */
	if (i == 0 || text_check_identifier(string[i]))
		return -1;
	return i;
}

static int txtfmt_lua_find_bool(const char *string)
{
	int i, len;

	if      (STR_LITERAL_STARTSWITH(string, "nil",   len))  i = len;
	else if (STR_LITERAL_STARTSWITH(string, "true",  len))  i = len;
	else if (STR_LITERAL_STARTSWITH(string, "false", len))  i = len;
	else                                                    i = 0;

	/* If next source char is an identifier (eg. 'i' in "Nonetheless") no match */
	if (i == 0 || text_check_identifier(string[i]))
		return -1;
	return i;
}

static char txtfmt_lua_format_identifier(const char *str)
{
	char fmt;
	if      ((txtfmt_lua_find_specialvar(str))  != -1) fmt = FMT_TYPE_SPECIAL;
	else if ((txtfmt_lua_find_keyword(str))     != -1) fmt = FMT_TYPE_KEYWORD;
	else                                               fmt = FMT_TYPE_DEFAULT;
	return fmt;
}

static void txtfmt_lua_format_line(SpaceText *st, TextLine *line, const bool do_next)
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
			/* Multi-line comments */
			if (cont & FMT_CONT_COMMENT_C) {
				if (*str == ']' && *(str + 1) == ']') {
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
			/* Multi-line comments */
			if (*str == '-'       && *(str + 1) == '-' &&
			    *(str + 2) == '[' && *(str + 3) == '[')
			{
				cont = FMT_CONT_COMMENT_C;
				*fmt = FMT_TYPE_COMMENT; fmt++; str++;
				*fmt = FMT_TYPE_COMMENT; fmt++; str++;
				*fmt = FMT_TYPE_COMMENT; fmt++; str++;
				*fmt = FMT_TYPE_COMMENT;
			}
			/* Single line comment */
			else if (*str == '-' && *(str + 1) == '-') {
				text_format_fill(&str, &fmt, FMT_TYPE_COMMENT, len - (int)(fmt - line->format));
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
			/* Booleans */
			else if (prev != FMT_TYPE_DEFAULT && (i = txtfmt_lua_find_bool(str)) != -1) {
				if (i > 0) {
					text_format_fill_ascii(&str, &fmt, FMT_TYPE_NUMERAL, i);
				}
				else {
					str += BLI_str_utf8_size_safe(str) - 1;
					*fmt = FMT_TYPE_DEFAULT;
				}
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
				if      ((i = txtfmt_lua_find_specialvar(str))   != -1) prev = FMT_TYPE_SPECIAL;
				else if ((i = txtfmt_lua_find_keyword(str))      != -1) prev = FMT_TYPE_KEYWORD;

				if (i > 0) {
					text_format_fill_ascii(&str, &fmt, prev, i);
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
		txtfmt_lua_format_line(st, line->next, do_next);
	}

	flatten_string_free(&fs);
}

void ED_text_format_register_lua(void)
{
	static TextFormatType tft = {NULL};
	static const char *ext[] = {"lua", NULL};

	tft.format_identifier = txtfmt_lua_format_identifier;
	tft.format_line       = txtfmt_lua_format_line;
	tft.ext = ext;

	ED_text_format_register(&tft);
}
