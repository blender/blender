/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation */

/** \file
 * \ingroup bli
 */

#include <stdio.h>
#include <stdlib.h>

#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLI_string_cursor_utf8.h" /* own include */

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

typedef enum eStrCursorDelimType {
  STRCUR_DELIM_NONE,
  STRCUR_DELIM_ALPHANUMERIC,
  STRCUR_DELIM_PUNCT,
  STRCUR_DELIM_BRACE,
  STRCUR_DELIM_OPERATOR,
  STRCUR_DELIM_QUOTE,
  STRCUR_DELIM_WHITESPACE,
  STRCUR_DELIM_OTHER,
} eStrCursorDelimType;

static eStrCursorDelimType cursor_delim_type_unicode(const uint uch)
{
  switch (uch) {
    case ',':
    case '.':
      return STRCUR_DELIM_PUNCT;

    case '{':
    case '}':
    case '[':
    case ']':
    case '(':
    case ')':
      return STRCUR_DELIM_BRACE;

    case '+':
    case '-':
    case '=':
    case '~':
    case '%':
    case '/':
    case '<':
    case '>':
    case '^':
    case '*':
    case '&':
    case '|':
      return STRCUR_DELIM_OPERATOR;

    case '\'':
    case '\"':
      return STRCUR_DELIM_QUOTE;

    case ' ':
    case '\t':
    case '\n':
      return STRCUR_DELIM_WHITESPACE;

    case '\\':
    case '@':
    case '#':
    case '$':
    case ':':
    case ';':
    case '?':
    case '!':
    case 0xA3:        /* pound */
    case 0x80:        /* euro */
      /* case '_': */ /* special case, for python */
      return STRCUR_DELIM_OTHER;

    default:
      break;
  }
  return STRCUR_DELIM_ALPHANUMERIC; /* Not quite true, but ok for now */
}

static eStrCursorDelimType cursor_delim_type_utf8(const char *ch_utf8,
                                                  const size_t ch_utf8_len,
                                                  const int pos)
{
  /* for full unicode support we really need to have large lookup tables to figure
   * out what's what in every possible char set - and python, glib both have these. */
  size_t index = (size_t)pos;
  uint uch = BLI_str_utf8_as_unicode_step_or_error(ch_utf8, ch_utf8_len, &index);
  return cursor_delim_type_unicode(uch);
}

bool BLI_str_cursor_step_next_utf8(const char *str, size_t str_maxlen, int *pos)
{
  /* NOTE: Keep in sync with #BLI_str_cursor_step_next_utf32. */

  if ((*pos) >= (int)str_maxlen) {
    return false;
  }
  const char *str_end = str + (str_maxlen + 1);
  const char *str_pos = str + (*pos);
  const char *str_next = str_pos;
  do {
    str_next = BLI_str_find_next_char_utf8(str_next, str_end);
  } while (str_next < str_end && str_next[0] != 0 && BLI_str_utf8_char_width(str_next) < 1);
  (*pos) += (str_next - str_pos);
  if ((*pos) > (int)str_maxlen) {
    (*pos) = (int)str_maxlen;
  }

  return true;
}

bool BLI_str_cursor_step_prev_utf8(const char *str, size_t str_maxlen, int *pos)
{
  /* NOTE: Keep in sync with #BLI_str_cursor_step_prev_utf32. */

  if ((*pos) > 0 && (*pos) <= str_maxlen) {
    const char *str_pos = str + (*pos);
    const char *str_prev = str_pos;
    do {
      str_prev = BLI_str_find_prev_char_utf8(str_prev, str);
    } while (str_prev > str && BLI_str_utf8_char_width(str_prev) == 0);
    (*pos) -= (str_pos - str_prev);
    return true;
  }

  return false;
}

void BLI_str_cursor_step_utf8(const char *str,
                              size_t str_maxlen,
                              int *pos,
                              eStrCursorJumpDirection direction,
                              eStrCursorJumpType jump,
                              bool use_init_step)
{
  const int pos_orig = *pos;

  if (direction == STRCUR_DIR_NEXT) {
    if (use_init_step) {
      BLI_str_cursor_step_next_utf8(str, str_maxlen, pos);
    }
    else {
      BLI_assert(jump == STRCUR_JUMP_DELIM);
    }

    if (jump != STRCUR_JUMP_NONE) {
      const eStrCursorDelimType delim_type = (*pos) < str_maxlen ?
                                                 cursor_delim_type_utf8(str, str_maxlen, *pos) :
                                                 STRCUR_DELIM_NONE;
      /* jump between special characters (/,\,_,-, etc.),
       * look at function cursor_delim_type() for complete
       * list of special character, ctr -> */
      while ((*pos) < str_maxlen) {
        if (BLI_str_cursor_step_next_utf8(str, str_maxlen, pos)) {
          if (*pos == str_maxlen) {
            break;
          }
          if ((jump != STRCUR_JUMP_ALL) &&
              (delim_type != cursor_delim_type_utf8(str, str_maxlen, *pos))) {
            break;
          }
        }
        else {
          break; /* unlikely but just in case */
        }
      }
    }
  }
  else if (direction == STRCUR_DIR_PREV) {
    if (use_init_step) {
      BLI_str_cursor_step_prev_utf8(str, str_maxlen, pos);
    }
    else {
      BLI_assert(jump == STRCUR_JUMP_DELIM);
    }

    if (jump != STRCUR_JUMP_NONE) {
      const eStrCursorDelimType delim_type = (*pos) > 0 ? cursor_delim_type_utf8(
                                                              str, str_maxlen, *pos - 1) :
                                                          STRCUR_DELIM_NONE;
      /* jump between special characters (/,\,_,-, etc.),
       * look at function cursor_delim_type() for complete
       * list of special character, ctr -> */
      while ((*pos) > 0) {
        const int pos_prev = *pos;
        if (BLI_str_cursor_step_prev_utf8(str, str_maxlen, pos)) {
          if ((jump != STRCUR_JUMP_ALL) &&
              (delim_type != cursor_delim_type_utf8(str, str_maxlen, *pos))) {
            /* left only: compensate for index/change in direction */
            if ((pos_orig - (*pos)) >= 1) {
              *pos = pos_prev;
            }
            break;
          }
        }
        else {
          break;
        }
      }
    }
  }
  else {
    BLI_assert_unreachable();
  }
}

bool BLI_str_cursor_step_next_utf32(const char32_t *str, size_t str_maxlen, int *pos)
{
  /* NOTE: Keep in sync with #BLI_str_cursor_step_next_utf8. */

  if ((*pos) >= (int)str_maxlen) {
    return false;
  }
  do {
    (*pos)++;
  } while (*pos < (int)str_maxlen && str[*pos] != 0 && BLI_wcwidth(str[*pos]) == 0);

  return true;
}

bool BLI_str_cursor_step_prev_utf32(const char32_t *str, size_t UNUSED(str_maxlen), int *pos)
{
  /* NOTE: Keep in sync with #BLI_str_cursor_step_prev_utf8. */

  if ((*pos) <= 0) {
    return false;
  }
  do {
    (*pos)--;
  } while (*pos > 0 && BLI_wcwidth(str[*pos]) == 0);

  return true;
}

void BLI_str_cursor_step_utf32(const char32_t *str,
                               size_t str_maxlen,
                               int *pos,
                               eStrCursorJumpDirection direction,
                               eStrCursorJumpType jump,
                               bool use_init_step)
{
  const int pos_orig = *pos;

  if (direction == STRCUR_DIR_NEXT) {
    if (use_init_step) {
      BLI_str_cursor_step_next_utf32(str, str_maxlen, pos);
    }
    else {
      BLI_assert(jump == STRCUR_JUMP_DELIM);
    }

    if (jump != STRCUR_JUMP_NONE) {
      const eStrCursorDelimType delim_type = (*pos) < str_maxlen ?
                                                 cursor_delim_type_unicode((uint)str[*pos]) :
                                                 STRCUR_DELIM_NONE;
      /* jump between special characters (/,\,_,-, etc.),
       * look at function cursor_delim_type_unicode() for complete
       * list of special character, ctr -> */
      while ((*pos) < str_maxlen) {
        if (BLI_str_cursor_step_next_utf32(str, str_maxlen, pos)) {
          if ((jump != STRCUR_JUMP_ALL) &&
              (delim_type != cursor_delim_type_unicode((uint)str[*pos]))) {
            break;
          }
        }
        else {
          break; /* unlikely but just in case */
        }
      }
    }
  }
  else if (direction == STRCUR_DIR_PREV) {
    if (use_init_step) {
      BLI_str_cursor_step_prev_utf32(str, str_maxlen, pos);
    }
    else {
      BLI_assert(jump == STRCUR_JUMP_DELIM);
    }

    if (jump != STRCUR_JUMP_NONE) {
      const eStrCursorDelimType delim_type = (*pos) > 0 ?
                                                 cursor_delim_type_unicode((uint)str[(*pos) - 1]) :
                                                 STRCUR_DELIM_NONE;
      /* jump between special characters (/,\,_,-, etc.),
       * look at function cursor_delim_type() for complete
       * list of special character, ctr -> */
      while ((*pos) > 0) {
        const int pos_prev = *pos;
        if (BLI_str_cursor_step_prev_utf32(str, str_maxlen, pos)) {
          if ((jump != STRCUR_JUMP_ALL) &&
              (delim_type != cursor_delim_type_unicode((uint)str[*pos]))) {
            /* left only: compensate for index/change in direction */
            if ((pos_orig - (*pos)) >= 1) {
              *pos = pos_prev;
            }
            break;
          }
        }
        else {
          break;
        }
      }
    }
  }
  else {
    BLI_assert_unreachable();
  }
}

void BLI_str_cursor_step_bounds_utf8(
    const char *str, const size_t str_maxlen, int *pos, int *start, int *end)
{
  /* What type of characters are on either side of the current cursor position? */
  const eStrCursorDelimType prev = (*pos > 0) ? cursor_delim_type_utf8(str, str_maxlen, *pos - 1) :
                                                STRCUR_DELIM_NONE;
  const eStrCursorDelimType next = (*pos < str_maxlen) ?
                                       cursor_delim_type_utf8(str, str_maxlen, *pos) :
                                       STRCUR_DELIM_NONE;
  *start = *pos;
  *end = *pos;

  if (prev == next || ELEM(next, STRCUR_DELIM_WHITESPACE, STRCUR_DELIM_NONE)) {
    /* Expand backward if we are between similar content, before whitespace, or at end. */
    BLI_str_cursor_step_utf8(str, str_maxlen, start, STRCUR_DIR_PREV, STRCUR_JUMP_DELIM, false);
  }
  if (prev == next || ELEM(prev, STRCUR_DELIM_WHITESPACE, STRCUR_DELIM_NONE)) {
    /* Expand forward if we are between similar content, after whitespace, or at beginning. */
    BLI_str_cursor_step_utf8(str, str_maxlen, end, STRCUR_DIR_NEXT, STRCUR_JUMP_DELIM, false);
  }

  /* Move cursor position to the end of selection. */
  *pos = *end;
}

void BLI_str_cursor_step_bounds_utf32(
    const char32_t *str, const size_t str_maxlen, int *pos, int *start, int *end)
{
  /* What type of characters are on either side of the current cursor position? */
  const eStrCursorDelimType prev = (*pos > 0) ? cursor_delim_type_unicode(str[*pos - 1]) :
                                                STRCUR_DELIM_NONE;
  const eStrCursorDelimType next = (*pos < str_maxlen) ? cursor_delim_type_unicode(str[*pos]) :
                                                         STRCUR_DELIM_NONE;
  *start = *pos;
  *end = *pos;

  if (prev == next || ELEM(next, STRCUR_DELIM_WHITESPACE, STRCUR_DELIM_NONE)) {
    /* Expand backward if we are between similar content, before whitespace, or at end. */
    BLI_str_cursor_step_utf32(str, str_maxlen, start, STRCUR_DIR_PREV, STRCUR_JUMP_DELIM, false);
  }
  if (prev == next || ELEM(prev, STRCUR_DELIM_WHITESPACE, STRCUR_DELIM_NONE)) {
    /* Expand forward if we are between similar content, after whitespace, or at beginning. */
    BLI_str_cursor_step_utf32(str, str_maxlen, end, STRCUR_DIR_NEXT, STRCUR_JUMP_DELIM, false);
  }

  /* Move cursor position to the end of selection. */
  *pos = *end;
}
