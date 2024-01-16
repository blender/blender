/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <stdio.h>
#include <stdlib.h>

#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLI_string_cursor_utf8.h" /* own include */

#include "BLI_strict_flags.h"

/**
 * The category of character as returned by #cursor_delim_type_unicode.
 *
 * \note Don't compare with any values besides #STRCUR_DELIM_NONE as cursor motion
 * should only delimit on changes, not treat some groups differently.
 *
 * For range calculation the order prioritizes expansion direction,
 * when the cursor is between two different categories, "hug" the smaller values.
 * Where white-space gets lowest priority. See #BLI_str_cursor_step_bounds_utf8.
 * This is done so expanding the range at a word boundary always chooses the word instead
 * of the white-space before or after it.
 */
typedef enum eStrCursorDelimType {
  STRCUR_DELIM_NONE,
  STRCUR_DELIM_ALPHANUMERIC,
  STRCUR_DELIM_PUNCT,
  STRCUR_DELIM_BRACE,
  STRCUR_DELIM_OPERATOR,
  STRCUR_DELIM_QUOTE,
  STRCUR_DELIM_OTHER,
  STRCUR_DELIM_WHITESPACE,
} eStrCursorDelimType;

static eStrCursorDelimType cursor_delim_type_unicode(const uint uch)
{
  switch (uch) {
    case ',':
    case '.':
    case 0x2026: /* Horizontal ellipsis. */
    case 0x3002: /* CJK full width full stop. */
    case 0xFF0C: /* CJK full width comma. */
    case 0xFF61: /* CJK half width full stop. */
      return STRCUR_DELIM_PUNCT;

    case '{':
    case '}':
    case '[':
    case ']':
    case '(':
    case ')':
    case 0x3010: /* CJK full width left black lenticular bracket. */
    case 0x3011: /* CJK full width right black lenticular bracket. */
    case 0xFF08: /* CJK full width left parenthesis. */
    case 0xFF09: /* CJK full width right parenthesis. */
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
    case 0x2014: /* Em dash. */
    case 0x300A: /* CJK full width left double angle bracket. */
    case 0x300B: /* CJK full width right double angle bracket. */
    case 0xFF0F: /* CJK full width solidus (forward slash). */
    case 0xFF5E: /* CJK full width tilde. */
      return STRCUR_DELIM_OPERATOR;

    case '\'':
    case '\"':
    case '`':
    case 0xB4:   /* Acute accent. */
    case 0x2018: /* Left single quotation mark. */
    case 0x2019: /* Right single quotation mark. */
    case 0x201C: /* Left double quotation mark. */
    case 0x201D: /* Right double quotation mark. */
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
    case 0xA3:        /* Pound sign. */
    case 0x80:        /* Euro sign. */
    case 0x3001:      /* CJK ideographic comma. */
    case 0xFF01:      /* CJK full width exclamation mark. */
    case 0xFF64:      /* CJK half width ideographic comma. */
    case 0xFF65:      /* Katakana half width middle dot. */
    case 0xFF1A:      /* CJK full width colon. */
    case 0xFF1B:      /* CJK full width semicolon. */
    case 0xFF1F:      /* CJK full width question mark. */
      /* case '_': */ /* special case, for python */
      return STRCUR_DELIM_OTHER;

    default:
      break;
  }
  return STRCUR_DELIM_ALPHANUMERIC; /* Not quite true, but ok for now */
}

static eStrCursorDelimType cursor_delim_type_utf8(const char *ch_utf8,
                                                  const int ch_utf8_len,
                                                  const int pos)
{
  BLI_assert(ch_utf8_len >= 0);
  /* for full unicode support we really need to have large lookup tables to figure
   * out what's what in every possible char set - and python, glib both have these. */
  size_t index = (size_t)pos;
  uint uch = BLI_str_utf8_as_unicode_step_or_error(ch_utf8, (size_t)ch_utf8_len, &index);
  return cursor_delim_type_unicode(uch);
}

bool BLI_str_cursor_step_next_utf8(const char *str, const int str_maxlen, int *pos)
{
  /* NOTE: Keep in sync with #BLI_str_cursor_step_next_utf32. */
  BLI_assert(str_maxlen >= 0);
  BLI_assert(*pos >= 0);

  if (*pos >= str_maxlen) {
    return false;
  }
  const char *str_end = str + (str_maxlen + 1);
  const char *str_pos = str + *pos;
  const char *str_next = str_pos;
  do {
    str_next = BLI_str_find_next_char_utf8(str_next, str_end);
  } while ((str_next < str_end) && (str_next[0] != 0) &&
           (BLI_str_utf8_char_width_or_error(str_next) == 0));
  *pos += (int)(str_next - str_pos);
  if (*pos > str_maxlen) {
    *pos = str_maxlen;
  }

  return true;
}

bool BLI_str_cursor_step_prev_utf8(const char *str, const int str_maxlen, int *pos)
{
  /* NOTE: Keep in sync with #BLI_str_cursor_step_prev_utf32. */
  BLI_assert(str_maxlen >= 0);
  BLI_assert(*pos >= 0);

  if ((*pos > 0) && (*pos <= str_maxlen)) {
    const char *str_pos = str + *pos;
    const char *str_prev = str_pos;
    do {
      str_prev = BLI_str_find_prev_char_utf8(str_prev, str);
    } while ((str_prev > str) && (BLI_str_utf8_char_width_or_error(str_prev) == 0));
    *pos -= (int)(str_pos - str_prev);
    return true;
  }

  return false;
}

void BLI_str_cursor_step_utf8(const char *str,
                              const int str_maxlen,
                              int *pos,
                              eStrCursorJumpDirection direction,
                              eStrCursorJumpType jump,
                              bool use_init_step)
{
  BLI_assert(str_maxlen >= 0);
  const int pos_orig = *pos;

  if (direction == STRCUR_DIR_NEXT) {
    if (use_init_step) {
      BLI_str_cursor_step_next_utf8(str, str_maxlen, pos);
    }
    else {
      BLI_assert(jump == STRCUR_JUMP_DELIM);
    }

    if (jump != STRCUR_JUMP_NONE) {
      const eStrCursorDelimType delim_type = (*pos < str_maxlen) ?
                                                 cursor_delim_type_utf8(str, str_maxlen, *pos) :
                                                 STRCUR_DELIM_NONE;
      /* jump between special characters (/,\,_,-, etc.),
       * look at function cursor_delim_type() for complete
       * list of special character, ctr -> */
      while (*pos < str_maxlen) {
        if (BLI_str_cursor_step_next_utf8(str, str_maxlen, pos)) {
          if (*pos == str_maxlen) {
            break;
          }
          if ((jump != STRCUR_JUMP_ALL) &&
              (delim_type != cursor_delim_type_utf8(str, str_maxlen, *pos)))
          {
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
      const eStrCursorDelimType delim_type = (*pos > 0) ? cursor_delim_type_utf8(
                                                              str, str_maxlen, *pos - 1) :
                                                          STRCUR_DELIM_NONE;
      /* jump between special characters (/,\,_,-, etc.),
       * look at function cursor_delim_type() for complete
       * list of special character, ctr -> */
      while (*pos > 0) {
        const int pos_prev = *pos;
        if (BLI_str_cursor_step_prev_utf8(str, str_maxlen, pos)) {
          if ((jump != STRCUR_JUMP_ALL) &&
              (delim_type != cursor_delim_type_utf8(str, str_maxlen, *pos)))
          {
            /* left only: compensate for index/change in direction */
            if ((pos_orig - *pos) >= 1) {
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

bool BLI_str_cursor_step_next_utf32(const char32_t *str, const int str_maxlen, int *pos)
{
  /* NOTE: Keep in sync with #BLI_str_cursor_step_next_utf8. */
  BLI_assert(str_maxlen >= 0);
  BLI_assert(*pos >= 0);

  if (*pos >= str_maxlen) {
    return false;
  }
  do {
    (*pos)++;
  } while ((*pos < str_maxlen) && (str[*pos] != 0) && (BLI_wcwidth_or_error(str[*pos]) == 0));

  return true;
}

bool BLI_str_cursor_step_prev_utf32(const char32_t *str, const int str_maxlen, int *pos)
{
  /* NOTE: Keep in sync with #BLI_str_cursor_step_prev_utf8. */
  BLI_assert(str_maxlen >= 0);
  BLI_assert(*pos >= 0);
  UNUSED_VARS_NDEBUG(str_maxlen);

  if (*pos <= 0) {
    return false;
  }
  do {
    (*pos)--;
  } while ((*pos > 0) && (BLI_wcwidth_or_error(str[*pos]) == 0));

  return true;
}

void BLI_str_cursor_step_utf32(const char32_t *str,
                               const int str_maxlen,
                               int *pos,
                               eStrCursorJumpDirection direction,
                               eStrCursorJumpType jump,
                               bool use_init_step)
{
  BLI_assert(str_maxlen >= 0);
  const int pos_orig = *pos;

  if (direction == STRCUR_DIR_NEXT) {
    if (use_init_step) {
      BLI_str_cursor_step_next_utf32(str, str_maxlen, pos);
    }
    else {
      BLI_assert(jump == STRCUR_JUMP_DELIM);
    }

    if (jump != STRCUR_JUMP_NONE) {
      const eStrCursorDelimType delim_type = (*pos < str_maxlen) ?
                                                 cursor_delim_type_unicode((uint)str[*pos]) :
                                                 STRCUR_DELIM_NONE;
      /* jump between special characters (/,\,_,-, etc.),
       * look at function cursor_delim_type_unicode() for complete
       * list of special character, ctr -> */
      while (*pos < str_maxlen) {
        if (BLI_str_cursor_step_next_utf32(str, str_maxlen, pos)) {
          if ((jump != STRCUR_JUMP_ALL) &&
              (delim_type != cursor_delim_type_unicode((uint)str[*pos])))
          {
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
      const eStrCursorDelimType delim_type = (*pos > 0) ?
                                                 cursor_delim_type_unicode((uint)str[(*pos) - 1]) :
                                                 STRCUR_DELIM_NONE;
      /* jump between special characters (/,\,_,-, etc.),
       * look at function cursor_delim_type() for complete
       * list of special character, ctr -> */
      while (*pos > 0) {
        const int pos_prev = *pos;
        if (BLI_str_cursor_step_prev_utf32(str, str_maxlen, pos)) {
          if ((jump != STRCUR_JUMP_ALL) &&
              (delim_type != cursor_delim_type_unicode((uint)str[*pos])))
          {
            /* left only: compensate for index/change in direction */
            if ((pos_orig - *pos) >= 1) {
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
    const char *str, const int str_maxlen, const int pos, int *r_start, int *r_end)
{
  BLI_assert(str_maxlen >= 0);
  BLI_assert(pos >= 0 && pos <= str_maxlen);
  /* Identify the type of characters are on either side of the current cursor position. */
  const eStrCursorDelimType prev = (pos > 0) ? cursor_delim_type_utf8(str, str_maxlen, pos - 1) :
                                               STRCUR_DELIM_NONE;
  const eStrCursorDelimType next = (pos < str_maxlen) ?
                                       cursor_delim_type_utf8(str, str_maxlen, pos) :
                                       STRCUR_DELIM_NONE;
  *r_start = pos;
  *r_end = pos;

  if (prev != STRCUR_DELIM_NONE) {
    if ((prev <= next) || (next == STRCUR_DELIM_NONE)) {
      /* Expand backward if we are between similar content. */
      BLI_str_cursor_step_utf8(
          str, str_maxlen, r_start, STRCUR_DIR_PREV, STRCUR_JUMP_DELIM, false);
    }
  }
  if (next != STRCUR_DELIM_NONE) {
    if ((next <= prev) || (prev == STRCUR_DELIM_NONE)) {
      /* Expand forward if we are between similar content. */
      BLI_str_cursor_step_utf8(str, str_maxlen, r_end, STRCUR_DIR_NEXT, STRCUR_JUMP_DELIM, false);
    }
  }
}

void BLI_str_cursor_step_bounds_utf32(
    const char32_t *str, const int str_maxlen, const int pos, int *r_start, int *r_end)
{
  BLI_assert(str_maxlen >= 0);
  BLI_assert(pos >= 0 && pos <= str_maxlen);
  /* Identify the type of characters are on either side of the current cursor position. */
  const eStrCursorDelimType prev = (pos > 0) ? cursor_delim_type_unicode(str[pos - 1]) :
                                               STRCUR_DELIM_NONE;
  const eStrCursorDelimType next = (pos < str_maxlen) ? cursor_delim_type_unicode(str[pos]) :
                                                        STRCUR_DELIM_NONE;
  *r_start = pos;
  *r_end = pos;

  if (prev != STRCUR_DELIM_NONE) {
    if ((prev <= next) || (next == STRCUR_DELIM_NONE)) {
      /* Expand backward if we are between similar content. */
      BLI_str_cursor_step_utf32(
          str, str_maxlen, r_start, STRCUR_DIR_PREV, STRCUR_JUMP_DELIM, false);
    }
  }
  if (next != STRCUR_DELIM_NONE) {
    if ((next <= prev) || (prev == STRCUR_DELIM_NONE)) {
      /* Expand forward if we are between similar content. */
      BLI_str_cursor_step_utf32(str, str_maxlen, r_end, STRCUR_DIR_NEXT, STRCUR_JUMP_DELIM, false);
    }
  }
}
