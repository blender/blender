/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptext
 */

#include <cstring>

#include "BLI_blenlib.h"

#include "DNA_space_types.h"
#include "DNA_text_types.h"

#include "BKE_text.h"

#include "text_format.hh"

/* -------------------------------------------------------------------- */
/** \name Local Literal Definitions
 * \{ */

/**
 * The following items are derived from this list:
 * \code{.py}
 * ", ".join(['"%s"' % kw
 *            for kw in sorted(__import__("keyword").kwlist + __import__("keyword").softkwlist)
 *            if kw not in {"False", "None", "True", "def", "class", "_"}])
 * \endcode
 *
 * The code below can be re-generated using:
 * \code{.py}
 * import keyword
 * ignore = {"False", "None", "True", "def", "class", "_"}
 * keywords = sorted(set(keyword.kwlist + keyword.softkwlist) - ignore)
 * longest = max(len(kw) for kw in keywords)
 * first  = 'if        (STR_LITERAL_STARTSWITH(string, "%s",%s len)) { i = len;'
 * middle = '} else if (STR_LITERAL_STARTSWITH(string, "%s",%s len)) { i = len;'
 * last   = '} else                                         %s       { i = 0;'
 * print("\n".join([(first if i==0 else middle) % (kw, ' '*(longest - len(kw)))
 *                 for (i, kw) in enumerate(keywords)]) + "\n" +
 *       last % (' '*(longest-2)) + "\n" +
 *       "}")
 * \endcode
 *
 * Python built-in function name.
 * See:
 * http://docs.python.org/py3k/reference/lexical_analysis.html#keywords
 */
static const char *text_format_py_literals_builtinfunc_data[] = {
    /* Force single column, sorted list. */
    /* clang-format off */
    "and",
    "as",
    "assert",
    "async",
    "await",
    "break",
    "case",
    "continue",
    "del",
    "elif",
    "else",
    "except",
    "finally",
    "for",
    "from",
    "global",
    "if",
    "import",
    "in",
    "is",
    "lambda",
    "match",
    "nonlocal",
    "not",
    "or",
    "pass",
    "raise",
    "return",
    "try",
    "while",
    "with",
    "yield",
    /* clang-format on */
};
static const Span<const char *> text_format_py_literals_builtinfunc(
    text_format_py_literals_builtinfunc_data,
    ARRAY_SIZE(text_format_py_literals_builtinfunc_data));

/** Python special name. */
static const char *text_format_py_literals_specialvar_data[] = {
    /* Force single column, sorted list. */
    /* clang-format off */
    "class",
    "def",
    /* clang-format on */
};
static const Span<const char *> text_format_py_literals_specialvar(
    text_format_py_literals_specialvar_data, ARRAY_SIZE(text_format_py_literals_specialvar_data));

/** Python bool values. */
static const char *text_format_py_literals_bool_data[] = {
    /* Force single column, sorted list. */
    /* clang-format off */
    "False",
    "None",
    "True",
    /* clang-format on */
};
static const Span<const char *> text_format_py_literals_bool(
    text_format_py_literals_bool_data, ARRAY_SIZE(text_format_py_literals_bool_data));

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local Functions (for #TextFormatType::format_line)
 * \{ */

static int txtfmt_py_find_builtinfunc(const char *string)
{
  const int i = text_format_string_literal_find(text_format_py_literals_builtinfunc, string);

  /* If next source char is an identifier (eg. 'i' in "definite") no match */
  if (i == 0 || text_check_identifier(string[i])) {
    return -1;
  }
  return i;
}

static int txtfmt_py_find_specialvar(const char *string)
{
  const int i = text_format_string_literal_find(text_format_py_literals_specialvar, string);

  /* If next source char is an identifier (eg. 'i' in "definite") no match */
  if (i == 0 || text_check_identifier(string[i])) {
    return -1;
  }
  return i;
}

static int txtfmt_py_find_decorator(const char *string)
{
  if (string[0] != '@') {
    return -1;
  }
  if (!text_check_identifier(string[1])) {
    return -1;
  }
  /* Interpret as matrix multiplication when followed by whitespace. */
  if (text_check_whitespace(string[1])) {
    return -1;
  }

  int i = 1;
  while (text_check_identifier(string[i])) {
    i++;
  }
  return i;
}

static int txtfmt_py_find_bool(const char *string)
{
  const int i = text_format_string_literal_find(text_format_py_literals_bool, string);

  /* If next source char is an identifier (eg. 'i' in "Nonetheless") no match */
  if (i == 0 || text_check_identifier(string[i])) {
    return -1;
  }
  return i;
}

/* Numeral character matching. */
#define TXTFMT_PY_NUMERAL_STRING_COUNT_IMPL(txtfmt_py_numeral_char_is_fn) \
  { \
    uint count = 0; \
    for (; txtfmt_py_numeral_char_is_fn(*string); string += 1) { \
      count += 1; \
    } \
    return count; \
  } \
  ((void)0)

/* Binary. */
static bool txtfmt_py_numeral_char_is_binary(const char c)
{
  return ELEM(c, '0', '1') || (c == '_');
}
static uint txtfmt_py_numeral_string_count_binary(const char *string)
{
  TXTFMT_PY_NUMERAL_STRING_COUNT_IMPL(txtfmt_py_numeral_char_is_binary);
}

/* Octal. */
static bool txtfmt_py_numeral_char_is_octal(const char c)
{
  return (c >= '0' && c <= '7') || (c == '_');
}
static uint txtfmt_py_numeral_string_count_octal(const char *string)
{
  TXTFMT_PY_NUMERAL_STRING_COUNT_IMPL(txtfmt_py_numeral_char_is_octal);
}

/* Decimal. */
static bool txtfmt_py_numeral_char_is_decimal(const char c)
{
  return (c >= '0' && c <= '9') || (c == '_');
}
static uint txtfmt_py_numeral_string_count_decimal(const char *string)
{
  TXTFMT_PY_NUMERAL_STRING_COUNT_IMPL(txtfmt_py_numeral_char_is_decimal);
}

/* Hexadecimal. */
static bool txtfmt_py_numeral_char_is_hexadecimal(const char c)
{
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || (c == '_');
}
static uint txtfmt_py_numeral_string_count_hexadecimal(const char *string)
{
  TXTFMT_PY_NUMERAL_STRING_COUNT_IMPL(txtfmt_py_numeral_char_is_hexadecimal);
}

/* Zeros. */
static bool txtfmt_py_numeral_char_is_zero(const char c)
{
  return ELEM(c, '0', '_');
}
static uint txtfmt_py_numeral_string_count_zeros(const char *string)
{
  TXTFMT_PY_NUMERAL_STRING_COUNT_IMPL(txtfmt_py_numeral_char_is_zero);
}

#undef TXTFMT_PY_NUMERAL_STRING_COUNT_IMPL

static int txtfmt_py_find_numeral_inner(const char *string)
{
  if (string == nullptr || *string == '\0') {
    return -1;
  }

  const char first = *string, second = *(string + 1);

  /* Decimal dot must be followed by a digit, any decimal digit.
   * Note that the there can be any number of leading zeros after
   * the decimal point (leading zeros are not allowed in integers) */
  if (first == '.') {
    if (text_check_digit(second)) {
      return 1 + txtfmt_py_numeral_string_count_decimal(string + 1);
    }
  }
  else if (first == '0') {
    /* Numerals starting with '0x' or '0X' is followed by hexadecimal digits. */
    if (ELEM(second, 'x', 'X')) {
      return 2 + txtfmt_py_numeral_string_count_hexadecimal(string + 2);
    }
    /* Numerals starting with '0o' or '0O' is followed by octal digits. */
    if (ELEM(second, 'o', 'O')) {
      return 2 + txtfmt_py_numeral_string_count_octal(string + 2);
    }
    /* Numerals starting with '0b' or '0B' is followed by binary digits. */
    if (ELEM(second, 'b', 'B')) {
      return 2 + txtfmt_py_numeral_string_count_binary(string + 2);
    }
    /* Other numerals starting with '0' can be followed by any number of '0' characters. */
    if (ELEM(second, '0', '_')) {
      return 2 + txtfmt_py_numeral_string_count_zeros(string + 2);
    }
  }
  /* Any non-zero digit is the start of a decimal number. */
  else if (first > '0' && first <= '9') {
    return 1 + txtfmt_py_numeral_string_count_decimal(string + 1);
  }
  /* A single zero is also allowed. */
  return (first == '0') ? 1 : 0;
}

static int txtfmt_py_literal_numeral(const char *string, char prev_fmt)
{
  if (string == nullptr || *string == '\0') {
    return -1;
  }

  const char first = *string, second = *(string + 1);

  if (prev_fmt == FMT_TYPE_NUMERAL) {
    /* Previous was a number; if immediately followed by 'e' or 'E' and a digit,
     * it's a base 10 exponent (scientific notation). */
    if (ELEM(first, 'e', 'E') && (text_check_digit(second) || second == '-')) {
      return 1 + txtfmt_py_find_numeral_inner(string + 1);
    }
    /* Previous was a number; if immediately followed by '.' it's a floating point decimal number.
     * NOTE: keep the decimal point, it's needed to allow leading zeros. */
    if (first == '.') {
      return txtfmt_py_find_numeral_inner(string);
    }
    /* "Imaginary" part of a complex number ends with 'j' */
    if (ELEM(first, 'j', 'J') && !text_check_digit(second)) {
      return 1;
    }
  }
  else if ((prev_fmt != FMT_TYPE_DEFAULT) &&
           (text_check_digit(first) || (first == '.' && text_check_digit(second))))
  {
    /* New numeral, starting with a digit or a decimal point followed by a digit. */
    return txtfmt_py_find_numeral_inner(string);
  }
  /* Not a literal numeral. */
  return 0;
}

static char txtfmt_py_format_identifier(const char *str)
{
  char fmt;

  /* Keep aligned args for readability. */
  /* clang-format off */

  if        (txtfmt_py_find_specialvar(str)   != -1) { fmt = FMT_TYPE_SPECIAL;
  } else if (txtfmt_py_find_builtinfunc(str)  != -1) { fmt = FMT_TYPE_KEYWORD;
  } else if (txtfmt_py_find_decorator(str)    != -1) { fmt = FMT_TYPE_RESERVED;
  } else                                             { fmt = FMT_TYPE_DEFAULT;
  }

  /* clang-format on */
  return fmt;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format Line Implementation (#TextFormatType::format_line)
 * \{ */

static void txtfmt_py_format_line(SpaceText *st, TextLine *line, const bool do_next)
{
  FlattenString fs;
  const char *str;
  char *fmt;
  char cont_orig, cont, find, prev = ' ';
  int len, i;

  /* Get continuation from previous line */
  if (line->prev && line->prev->format != nullptr) {
    fmt = line->prev->format;
    cont = fmt[strlen(fmt) + 1]; /* Just after the null-terminator */
    BLI_assert((FMT_CONT_ALL & cont) == cont);
    /* So slashes beginning on continuation display properly, see: #118767. */
    if (cont & (FMT_CONT_QUOTEDOUBLE | FMT_CONT_QUOTESINGLE | FMT_CONT_TRIPLE)) {
      prev = FMT_TYPE_STRING;
 }
  }
  else {
    cont = FMT_CONT_NOP;
  }

  /* Get original continuation from this line */
  if (line->format != nullptr) {
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
      *fmt = prev;
      fmt++;
      str++;
      if (*str == '\0') {
        break;
      }
      *fmt = prev;
      fmt++;
      str += BLI_str_utf8_size_safe(str);
      continue;
    }
    /* Handle continuations */
    if (cont) {
      /* Triple strings ("""...""" or '''...''') */
      if (cont & FMT_CONT_TRIPLE) {
        find = (cont & FMT_CONT_QUOTEDOUBLE) ? '"' : '\'';
        if (*str == find && *(str + 1) == find && *(str + 2) == find) {
          *fmt = FMT_TYPE_STRING;
          fmt++;
          str++;
          *fmt = FMT_TYPE_STRING;
          fmt++;
          str++;
          cont = FMT_CONT_NOP;
        }
        /* Handle other strings */
      }
      else {
        find = (cont & FMT_CONT_QUOTEDOUBLE) ? '"' : '\'';
        if (*str == find) {
          cont = FMT_CONT_NOP;
        }
      }

      *fmt = FMT_TYPE_STRING;
      str += BLI_str_utf8_size_safe(str) - 1;
    }
    /* Not in a string... */
    else {
      /* Deal with comments first */
      if (*str == '#') {
        /* fill the remaining line */
        text_format_fill(&str, &fmt, FMT_TYPE_COMMENT, len - int(fmt - line->format));
      }
      else if (ELEM(*str, '"', '\'')) {
        /* Strings */
        find = *str;
        cont = (*str == '"') ? FMT_CONT_QUOTEDOUBLE : FMT_CONT_QUOTESINGLE;
        if (*(str + 1) == find && *(str + 2) == find) {
          *fmt = FMT_TYPE_STRING;
          fmt++;
          str++;
          *fmt = FMT_TYPE_STRING;
          fmt++;
          str++;
          cont |= FMT_CONT_TRIPLE;
        }
        *fmt = FMT_TYPE_STRING;
      }
      else if (ELEM(*str, 'f', 'F', 'r', 'R', 'u', 'U') && ELEM(*(str + 1), '"', '\'')) {
        /* Strings with single letter prefixes (f-strings, raw strings, and unicode strings).
         * Format the prefix as part of the string. */
        *fmt = FMT_TYPE_STRING;
        fmt++;
        str++;
        find = *str;
        cont = (*str == '"') ? FMT_CONT_QUOTEDOUBLE : FMT_CONT_QUOTESINGLE;
        if (*(str + 1) == find && *(str + 2) == find) {
          *fmt = FMT_TYPE_STRING;
          fmt++;
          str++;
          *fmt = FMT_TYPE_STRING;
          fmt++;
          str++;
          cont |= FMT_CONT_TRIPLE;
        }
        *fmt = FMT_TYPE_STRING;
      }
      else if (((ELEM(*str, 'f', 'F') && ELEM(*(str + 1), 'r', 'R')) ||
                (ELEM(*str, 'r', 'R') && ELEM(*(str + 1), 'f', 'F'))) &&
               ELEM(*(str + 2), '"', '\''))
      {
        /* Strings with two letter prefixes (raw f-strings).
         * Format the prefix as part of the string. */
        *fmt = FMT_TYPE_STRING;
        fmt++;
        str++;
        *fmt = FMT_TYPE_STRING;
        fmt++;
        str++;
        find = *str;
        cont = (*str == '"') ? FMT_CONT_QUOTEDOUBLE : FMT_CONT_QUOTESINGLE;
        if (*(str + 1) == find && *(str + 2) == find) {
          *fmt = FMT_TYPE_STRING;
          fmt++;
          str++;
          *fmt = FMT_TYPE_STRING;
          fmt++;
          str++;
          cont |= FMT_CONT_TRIPLE;
        }
        *fmt = FMT_TYPE_STRING;
      }
      /* White-space (all white-space has been converted to spaces). */
      else if (*str == ' ') {
        *fmt = FMT_TYPE_WHITESPACE;
      }
      /* Literal numerals, "numbers". */
      else if ((i = txtfmt_py_literal_numeral(str, prev)) > 0) {
        text_format_fill(&str, &fmt, FMT_TYPE_NUMERAL, i);
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
      /* Identifiers and other text (no previous white-space/delimiters so text continues). */
      else if (prev == FMT_TYPE_DEFAULT) {
        str += BLI_str_utf8_size_safe(str) - 1;
        *fmt = FMT_TYPE_DEFAULT;
      }
      /* Not white-space, a digit, punctuation, or continuing text.
       * Must be new, check for special words. */
      else {
        /* Keep aligned arguments for readability. */
        /* clang-format off */

        /* Special vars(v) or built-in keywords(b) */
        /* keep in sync with `txtfmt_py_format_identifier()`. */
        if        ((i = txtfmt_py_find_specialvar(str))   != -1) { prev = FMT_TYPE_SPECIAL;
        } else if ((i = txtfmt_py_find_builtinfunc(str))  != -1) { prev = FMT_TYPE_KEYWORD;
        } else if ((i = txtfmt_py_find_decorator(str))    != -1) { prev = FMT_TYPE_DIRECTIVE;
        }

        /* clang-format on */

        if (i > 0) {
          if (prev == FMT_TYPE_DIRECTIVE) { /* can contain utf8 */
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
    prev = *fmt;
    fmt++;
    str++;
  }

  /* Terminate and add continuation char */
  *fmt = '\0';
  fmt++;
  *fmt = cont;

  /* If continuation has changed and we're allowed, process the next line */
  if (cont != cont_orig && do_next && line->next) {
    txtfmt_py_format_line(st, line->next, do_next);
  }

  flatten_string_free(&fs);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_text_format_register_py()
{
  static TextFormatType tft = {nullptr};
  static const char *ext[] = {"py", nullptr};

  tft.format_identifier = txtfmt_py_format_identifier;
  tft.format_line = txtfmt_py_format_line;
  tft.ext = ext;
  tft.comment_line = "#";

  ED_text_format_register(&tft);

  BLI_assert(text_format_string_literals_check_sorted_array(text_format_py_literals_builtinfunc));
  BLI_assert(text_format_string_literals_check_sorted_array(text_format_py_literals_specialvar));
  BLI_assert(text_format_string_literals_check_sorted_array(text_format_py_literals_bool));
}

/** \} */
