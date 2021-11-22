/*
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
 */

/** \file
 * \ingroup sptext
 */

#pragma once

/* *** Flatten String *** */
typedef struct FlattenString {
  char fixedbuf[256];
  int fixedaccum[256];

  char *buf;
  int *accum;
  int pos, len;
} FlattenString;

/* format continuation flags (stored just after the NULL terminator) */
enum {
  FMT_CONT_NOP = 0,                /* no continuation */
  FMT_CONT_QUOTESINGLE = (1 << 0), /* single quotes */
  FMT_CONT_QUOTEDOUBLE = (1 << 1), /* double quotes */
  FMT_CONT_TRIPLE = (1 << 2),      /* triplets of quotes: """ or ''' */
  FMT_CONT_QUOTESINGLE_TRIPLE = (FMT_CONT_TRIPLE | FMT_CONT_QUOTESINGLE),
  FMT_CONT_QUOTEDOUBLE_TRIPLE = (FMT_CONT_TRIPLE | FMT_CONT_QUOTEDOUBLE),
  FMT_CONT_COMMENT_C = (1 << 3) /* multi-line comments, OSL only (C style) */
};
#define FMT_CONT_ALL \
  (FMT_CONT_QUOTESINGLE | FMT_CONT_QUOTEDOUBLE | FMT_CONT_TRIPLE | FMT_CONT_COMMENT_C)

int flatten_string(const struct SpaceText *st, FlattenString *fs, const char *in);
void flatten_string_free(FlattenString *fs);
int flatten_string_strlen(FlattenString *fs, const char *str);

int text_check_format_len(TextLine *line, unsigned int len);
void text_format_fill(const char **str_p, char **fmt_p, const char type, const int len);
void text_format_fill_ascii(const char **str_p, char **fmt_p, const char type, const int len);

/* *** Generalize Formatting *** */
typedef struct TextFormatType {
  struct TextFormatType *next, *prev;

  char (*format_identifier)(const char *string);

  /* Formats the specified line. If do_next is set, the process will move on to
   * the succeeding line if it is affected (eg. multi-line strings). Format strings
   * may contain any of the following characters:
   *
   * It is terminated with a null-terminator '\0' followed by a continuation
   * flag indicating whether the line is part of a multi-line string.
   *
   * See: FMT_TYPE_ enums below
   */
  void (*format_line)(SpaceText *st, TextLine *line, const bool do_next);

  const char **ext; /* NULL terminated extensions */
} TextFormatType;

enum {
  /** White-space */
  FMT_TYPE_WHITESPACE = '_',
  /** Comment text */
  FMT_TYPE_COMMENT = '#',
  /** Punctuation and other symbols */
  FMT_TYPE_SYMBOL = '!',
  /** Numerals */
  FMT_TYPE_NUMERAL = 'n',
  /** String letters */
  FMT_TYPE_STRING = 'l',
  /** Decorator / Pre-processor directive */
  FMT_TYPE_DIRECTIVE = 'd',
  /** Special variables (class, def) */
  FMT_TYPE_SPECIAL = 'v',
  /** Reserved keywords currently not in use, but still prohibited (OSL -> switch e.g.) */
  FMT_TYPE_RESERVED = 'r',
  /** Built-in names (return, for, etc.) */
  FMT_TYPE_KEYWORD = 'b',
  /** Regular text (identifiers, etc.) */
  FMT_TYPE_DEFAULT = 'q',
};

TextFormatType *ED_text_format_get(Text *text);
void ED_text_format_register(TextFormatType *tft);

/* formatters */
void ED_text_format_register_py(void);
void ED_text_format_register_osl(void);
void ED_text_format_register_lua(void);
void ED_text_format_register_pov(void);
void ED_text_format_register_pov_ini(void);

#define STR_LITERAL_STARTSWITH(str, str_literal, len_var) \
  (strncmp(str, str_literal, len_var = (sizeof(str_literal) - 1)) == 0)
