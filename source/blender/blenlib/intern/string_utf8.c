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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * Code from gutf8.c Copyright (C) 1999 Tom Tromey
 *                   Copyright (C) 2000 Red Hat, Inc.
 * All rights reserved.
 */

/** \file
 * \ingroup bli
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <wcwidth.h>

#include "BLI_utildefines.h"

#include "BLI_string_utf8.h" /* own include */
#ifdef WIN32
#  include "utfconv.h"
#endif
#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

// #define DEBUG_STRSIZE

/* array copied from glib's gutf8.c, */
/* Note: last two values (0xfe and 0xff) are forbidden in utf-8,
 * so they are considered 1 byte length too. */
static const size_t utf8_skip_data[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 1, 1,
};

/* from libswish3, originally called u8_isvalid(),
 * modified to return the index of the bad character (byte index not utf).
 * http://svn.swish-e.org/libswish3/trunk/src/libswish3/utf8.c r3044 - campbell */

/* based on the valid_utf8 routine from the PCRE library by Philip Hazel
 *
 * length is in bytes, since without knowing whether the string is valid
 * it's hard to know how many characters there are! */

/**
 * Find first utf-8 invalid byte in given \a str, of \a length bytes.
 *
 * \return the offset of the first invalid byte.
 */
ptrdiff_t BLI_utf8_invalid_byte(const char *str, size_t length)
{
  const unsigned char *p, *perr, *pend = (const unsigned char *)str + length;
  unsigned char c;
  int ab;

  for (p = (const unsigned char *)str; p < pend; p++, length--) {
    c = *p;
    perr = p; /* Erroneous char is always the first of an invalid utf8 sequence... */
    if (ELEM(c, 0xfe, 0xff, 0x00)) {
      /* Those three values are not allowed in utf8 string. */
      goto utf8_error;
    }
    if (c < 128) {
      continue;
    }
    if ((c & 0xc0) != 0xc0) {
      goto utf8_error;
    }

    /* Note that since we always increase p (and decrease length) by one byte in main loop,
     * we only add/subtract extra utf8 bytes in code below
     * (ab number, aka number of bytes remaining in the utf8 sequence after the initial one). */
    ab = (int)utf8_skip_data[c] - 1;
    if (length <= ab) {
      goto utf8_error;
    }

    /* Check top bits in the second byte */
    p++;
    length--;
    if ((*p & 0xc0) != 0x80) {
      goto utf8_error;
    }

    /* Check for overlong sequences for each different length */
    switch (ab) {
      case 1:
        /* Check for xx00 000x */
        if ((c & 0x3e) == 0) {
          goto utf8_error;
        }
        continue; /* We know there aren't any more bytes to check */

      case 2:
        /* Check for 1110 0000, xx0x xxxx */
        if (c == 0xe0 && (*p & 0x20) == 0) {
          goto utf8_error;
        }
        /* Some special cases, see section 5 of utf-8 decoder stress-test by Markus Kuhn
         * (https://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt). */
        /* From section 5.1 (and 5.2) */
        if (c == 0xed) {
          if (*p == 0xa0 && *(p + 1) == 0x80) {
            goto utf8_error;
          }
          if (*p == 0xad && *(p + 1) == 0xbf) {
            goto utf8_error;
          }
          if (*p == 0xae && *(p + 1) == 0x80) {
            goto utf8_error;
          }
          if (*p == 0xaf && *(p + 1) == 0xbf) {
            goto utf8_error;
          }
          if (*p == 0xb0 && *(p + 1) == 0x80) {
            goto utf8_error;
          }
          if (*p == 0xbe && *(p + 1) == 0x80) {
            goto utf8_error;
          }
          if (*p == 0xbf && *(p + 1) == 0xbf) {
            goto utf8_error;
          }
        }
        /* From section 5.3 */
        if (c == 0xef) {
          if (*p == 0xbf && *(p + 1) == 0xbe) {
            goto utf8_error;
          }
          if (*p == 0xbf && *(p + 1) == 0xbf) {
            goto utf8_error;
          }
        }
        break;

      case 3:
        /* Check for 1111 0000, xx00 xxxx */
        if (c == 0xf0 && (*p & 0x30) == 0) {
          goto utf8_error;
        }
        break;

      case 4:
        /* Check for 1111 1000, xx00 0xxx */
        if (c == 0xf8 && (*p & 0x38) == 0) {
          goto utf8_error;
        }
        break;

      case 5:
        /* Check for 1111 1100, xx00 00xx */
        if (c == 0xfc && (*p & 0x3c) == 0) {
          goto utf8_error;
        }
        break;
    }

    /* Check for valid bytes after the 2nd, if any; all must start 10 */
    while (--ab > 0) {
      p++;
      length--;
      if ((*p & 0xc0) != 0x80) {
        goto utf8_error;
      }
    }
  }

  return -1;

utf8_error:

  return ((const char *)perr - (const char *)str);
}

/**
 * Remove any invalid utf-8 byte (taking into account multi-bytes sequence of course).
 *
 * \return number of stripped bytes.
 */
int BLI_utf8_invalid_strip(char *str, size_t length)
{
  ptrdiff_t bad_char;
  int tot = 0;

  BLI_assert(str[length] == '\0');

  while ((bad_char = BLI_utf8_invalid_byte(str, length)) != -1) {
    str += bad_char;
    length -= (size_t)(bad_char + 1);

    if (length == 0) {
      /* last character bad, strip it */
      *str = '\0';
      tot++;
      break;
    }
    /* strip, keep looking */
    memmove(str, str + 1, length + 1); /* +1 for NULL char! */
    tot++;
  }

  return tot;
}

/** Compatible with #BLI_strncpy, but ensure no partial UTF8 chars. */
#define BLI_STR_UTF8_CPY(dst, src, maxncpy) \
  { \
    size_t utf8_size; \
    while (*src != '\0' && (utf8_size = utf8_skip_data[*src]) < maxncpy) { \
      maxncpy -= utf8_size; \
      switch (utf8_size) { \
        case 6: \
          *dst++ = *src++; \
          ATTR_FALLTHROUGH; \
        case 5: \
          *dst++ = *src++; \
          ATTR_FALLTHROUGH; \
        case 4: \
          *dst++ = *src++; \
          ATTR_FALLTHROUGH; \
        case 3: \
          *dst++ = *src++; \
          ATTR_FALLTHROUGH; \
        case 2: \
          *dst++ = *src++; \
          ATTR_FALLTHROUGH; \
        case 1: \
          *dst++ = *src++; \
      } \
    } \
    *dst = '\0'; \
  } \
  (void)0

char *BLI_strncpy_utf8(char *__restrict dst, const char *__restrict src, size_t maxncpy)
{
  char *r_dst = dst;

  BLI_assert(maxncpy != 0);

#ifdef DEBUG_STRSIZE
  memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif

  /* note: currently we don't attempt to deal with invalid utf8 chars */
  BLI_STR_UTF8_CPY(dst, src, maxncpy);

  return r_dst;
}

size_t BLI_strncpy_utf8_rlen(char *__restrict dst, const char *__restrict src, size_t maxncpy)
{
  char *r_dst = dst;

  BLI_assert(maxncpy != 0);

#ifdef DEBUG_STRSIZE
  memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif

  /* note: currently we don't attempt to deal with invalid utf8 chars */
  BLI_STR_UTF8_CPY(dst, src, maxncpy);

  return (size_t)(dst - r_dst);
}

#undef BLI_STR_UTF8_CPY

/* --------------------------------------------------------------------------*/
/* wchar_t / utf8 functions  */

size_t BLI_strncpy_wchar_as_utf8(char *__restrict dst,
                                 const wchar_t *__restrict src,
                                 const size_t maxncpy)
{
  const size_t maxlen = maxncpy - 1;
  /* 6 is max utf8 length of an unicode char. */
  const int64_t maxlen_secured = (int64_t)maxlen - 6;
  size_t len = 0;

  BLI_assert(maxncpy != 0);

#ifdef DEBUG_STRSIZE
  memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif

  while (*src && len <= maxlen_secured) {
    len += BLI_str_utf8_from_unicode((uint)*src++, dst + len);
  }

  /* We have to be more careful for the last six bytes,
   * to avoid buffer overflow in case utf8-encoded char would be too long for our dst buffer. */
  while (*src) {
    char t[6];
    size_t l = BLI_str_utf8_from_unicode((uint)*src++, t);
    BLI_assert(l <= 6);
    if (len + l > maxlen) {
      break;
    }
    memcpy(dst + len, t, l);
    len += l;
  }

  dst[len] = '\0';

  return len;
}

/* wchar len in utf8 */
size_t BLI_wstrlen_utf8(const wchar_t *src)
{
  size_t len = 0;

  while (*src) {
    len += BLI_str_utf8_from_unicode((uint)*src++, NULL);
  }

  return len;
}

size_t BLI_strlen_utf8_ex(const char *strc, size_t *r_len_bytes)
{
  size_t len;
  const char *strc_orig = strc;

  for (len = 0; *strc; len++) {
    strc += BLI_str_utf8_size_safe(strc);
  }

  *r_len_bytes = (size_t)(strc - strc_orig);
  return len;
}

size_t BLI_strlen_utf8(const char *strc)
{
  size_t len_bytes;
  return BLI_strlen_utf8_ex(strc, &len_bytes);
}

size_t BLI_strnlen_utf8_ex(const char *strc, const size_t maxlen, size_t *r_len_bytes)
{
  size_t len = 0;
  const char *strc_orig = strc;
  const char *strc_end = strc + maxlen;

  while (true) {
    size_t step = (size_t)BLI_str_utf8_size_safe(strc);
    if (!*strc || strc + step > strc_end) {
      break;
    }
    strc += step;
    len++;
  }

  *r_len_bytes = (size_t)(strc - strc_orig);
  return len;
}

/**
 * \param strc: the string to measure the length.
 * \param maxlen: the string length (in bytes)
 * \return the unicode length (not in bytes!)
 */
size_t BLI_strnlen_utf8(const char *strc, const size_t maxlen)
{
  size_t len_bytes;
  return BLI_strnlen_utf8_ex(strc, maxlen, &len_bytes);
}

size_t BLI_strncpy_wchar_from_utf8(wchar_t *__restrict dst_w,
                                   const char *__restrict src_c,
                                   const size_t maxncpy)
{
#ifdef WIN32
  return conv_utf_8_to_16(src_c, dst_w, maxncpy);
#else
  return BLI_str_utf8_as_utf32((char32_t *)dst_w, src_c, maxncpy);
#endif
}

/* end wchar_t / utf8 functions  */
/* --------------------------------------------------------------------------*/

/* count columns that character/string occupies, based on wcwidth.c */

int BLI_wcwidth(char32_t ucs)
{
  return mk_wcwidth(ucs);
}

int BLI_wcswidth(const char32_t *pwcs, size_t n)
{
  return mk_wcswidth(pwcs, n);
}

int BLI_str_utf8_char_width(const char *p)
{
  uint unicode = BLI_str_utf8_as_unicode(p);
  if (unicode == BLI_UTF8_ERR) {
    return -1;
  }

  return BLI_wcwidth((char32_t)unicode);
}

int BLI_str_utf8_char_width_safe(const char *p)
{
  int columns;

  uint unicode = BLI_str_utf8_as_unicode(p);
  if (unicode == BLI_UTF8_ERR) {
    return 1;
  }

  columns = BLI_wcwidth((char32_t)unicode);

  return (columns < 0) ? 1 : columns;
}

/* --------------------------------------------------------------------------*/

/* copied from glib's gutf8.c, added 'Err' arg */

/* note, glib uses uint for unicode, best we do the same,
 * though we don't typedef it - campbell */

#define UTF8_COMPUTE(Char, Mask, Len, Err) \
  if (Char < 128) { \
    Len = 1; \
    Mask = 0x7f; \
  } \
  else if ((Char & 0xe0) == 0xc0) { \
    Len = 2; \
    Mask = 0x1f; \
  } \
  else if ((Char & 0xf0) == 0xe0) { \
    Len = 3; \
    Mask = 0x0f; \
  } \
  else if ((Char & 0xf8) == 0xf0) { \
    Len = 4; \
    Mask = 0x07; \
  } \
  else if ((Char & 0xfc) == 0xf8) { \
    Len = 5; \
    Mask = 0x03; \
  } \
  else if ((Char & 0xfe) == 0xfc) { \
    Len = 6; \
    Mask = 0x01; \
  } \
  else { \
    Len = Err; /* -1 is the typical error value or 1 to skip */ \
  } \
  (void)0

/* same as glib define but added an 'Err' arg */
#define UTF8_GET(Result, Chars, Count, Mask, Len, Err) \
  (Result) = (Chars)[0] & (Mask); \
  for ((Count) = 1; (Count) < (Len); ++(Count)) { \
    if (((Chars)[(Count)] & 0xc0) != 0x80) { \
      (Result) = Err; \
      break; \
    } \
    (Result) <<= 6; \
    (Result) |= ((Chars)[(Count)] & 0x3f); \
  } \
  (void)0

/* uses glib functions but not from glib */
/* gets the size of a single utf8 char */
int BLI_str_utf8_size(const char *p)
{
  int mask = 0, len;
  const unsigned char c = (unsigned char)*p;

  UTF8_COMPUTE(c, mask, len, -1);

  (void)mask; /* quiet warning */

  return len;
}

/* use when we want to skip errors */
int BLI_str_utf8_size_safe(const char *p)
{
  int mask = 0, len;
  const unsigned char c = (unsigned char)*p;

  UTF8_COMPUTE(c, mask, len, 1);

  (void)mask; /* quiet warning */

  return len;
}

/* was g_utf8_get_char */
/**
 * BLI_str_utf8_as_unicode:
 * \param p: a pointer to Unicode character encoded as UTF-8
 *
 * Converts a sequence of bytes encoded as UTF-8 to a Unicode character.
 * If \a p does not point to a valid UTF-8 encoded character, results are
 * undefined. If you are not sure that the bytes are complete
 * valid Unicode characters, you should use g_utf8_get_char_validated()
 * instead.
 *
 * Return value: the resulting character
 */
uint BLI_str_utf8_as_unicode(const char *p)
{
  int i, len;
  uint mask = 0;
  uint result;
  const unsigned char c = (unsigned char)*p;

  UTF8_COMPUTE(c, mask, len, -1);
  if (UNLIKELY(len == -1)) {
    return BLI_UTF8_ERR;
  }
  UTF8_GET(result, p, i, mask, len, BLI_UTF8_ERR);

  return result;
}

/* variant that increments the length */
uint BLI_str_utf8_as_unicode_and_size(const char *__restrict p, size_t *__restrict index)
{
  int i, len;
  uint mask = 0;
  uint result;
  const unsigned char c = (unsigned char)*p;

  UTF8_COMPUTE(c, mask, len, -1);
  if (UNLIKELY(len == -1)) {
    return BLI_UTF8_ERR;
  }
  UTF8_GET(result, p, i, mask, len, BLI_UTF8_ERR);
  *index += (size_t)len;
  return result;
}

uint BLI_str_utf8_as_unicode_and_size_safe(const char *__restrict p, size_t *__restrict index)
{
  int i, len;
  uint mask = 0;
  uint result;
  const unsigned char c = (unsigned char)*p;

  UTF8_COMPUTE(c, mask, len, -1);
  if (UNLIKELY(len == -1)) {
    *index += 1;
    return c;
  }
  UTF8_GET(result, p, i, mask, len, BLI_UTF8_ERR);
  *index += (size_t)len;
  return result;
}

/* another variant that steps over the index,
 * note, currently this also falls back to latin1 for text drawing. */
uint BLI_str_utf8_as_unicode_step(const char *__restrict p, size_t *__restrict index)
{
  int i, len;
  uint mask = 0;
  uint result;
  unsigned char c;

  p += *index;
  c = (unsigned char)*p;

  UTF8_COMPUTE(c, mask, len, -1);
  if (UNLIKELY(len == -1)) {
    /* when called with NULL end, result will never be NULL,
     * checks for a NULL character */
    const char *p_next = BLI_str_find_next_char_utf8(p, NULL);
    /* will never return the same pointer unless '\0',
     * eternal loop is prevented */
    *index += (size_t)(p_next - p);
    return BLI_UTF8_ERR;
  }

  /* this is tricky since there are a few ways we can bail out of bad unicode
   * values, 3 possible solutions. */
#if 0
  UTF8_GET(result, p, i, mask, len, BLI_UTF8_ERR);
#elif 1
  /* WARNING: this is NOT part of glib, or supported by similar functions.
   * this is added for text drawing because some filepaths can have latin1
   * characters */
  UTF8_GET(result, p, i, mask, len, BLI_UTF8_ERR);
  if (result == BLI_UTF8_ERR) {
    len = 1;
    result = *p;
  }
  /* end warning! */
#else
  /* without a fallback like '?', text drawing will stop on this value */
  UTF8_GET(result, p, i, mask, len, '?');
#endif

  *index += (size_t)len;
  return result;
}

/* was g_unichar_to_utf8 */
/**
 * BLI_str_utf8_from_unicode:
 * \param c: a Unicode character code
 * \param outbuf: output buffer, must have at least 6 bytes of space.
 *       If %NULL, the length will be computed and returned
 *       and nothing will be written to outbuf.
 *
 * Converts a single character to UTF-8.
 *
 * \return number of bytes written
 */
size_t BLI_str_utf8_from_unicode(uint c, char *outbuf)
{
  /* If this gets modified, also update the copy in g_string_insert_unichar() */
  uint len = 0;
  uint first;
  uint i;

  if (c < 0x80) {
    first = 0;
    len = 1;
  }
  else if (c < 0x800) {
    first = 0xc0;
    len = 2;
  }
  else if (c < 0x10000) {
    first = 0xe0;
    len = 3;
  }
  else if (c < 0x200000) {
    first = 0xf0;
    len = 4;
  }
  else if (c < 0x4000000) {
    first = 0xf8;
    len = 5;
  }
  else {
    first = 0xfc;
    len = 6;
  }

  if (outbuf) {
    for (i = len - 1; i > 0; i--) {
      outbuf[i] = (c & 0x3f) | 0x80;
      c >>= 6;
    }
    outbuf[0] = c | first;
  }

  return len;
}

size_t BLI_str_utf8_as_utf32(char32_t *__restrict dst_w,
                             const char *__restrict src_c,
                             const size_t maxncpy)
{
  const size_t maxlen = maxncpy - 1;
  size_t len = 0;

  BLI_assert(maxncpy != 0);

#ifdef DEBUG_STRSIZE
  memset(dst_w, 0xff, sizeof(*dst_w) * maxncpy);
#endif

  while (*src_c && len != maxlen) {
    size_t step = 0;
    uint unicode = BLI_str_utf8_as_unicode_and_size(src_c, &step);
    if (unicode != BLI_UTF8_ERR) {
      *dst_w = unicode;
      src_c += step;
    }
    else {
      *dst_w = '?';
      src_c = BLI_str_find_next_char_utf8(src_c, NULL);
    }
    dst_w++;
    len++;
  }

  *dst_w = 0;

  return len;
}

size_t BLI_str_utf32_as_utf8(char *__restrict dst,
                             const char32_t *__restrict src,
                             const size_t maxncpy)
{
  const size_t maxlen = maxncpy - 1;
  /* 6 is max utf8 length of an unicode char. */
  const int64_t maxlen_secured = (int64_t)maxlen - 6;
  size_t len = 0;

  BLI_assert(maxncpy != 0);

#ifdef DEBUG_STRSIZE
  memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif

  while (*src && len <= maxlen_secured) {
    len += BLI_str_utf8_from_unicode((uint)*src++, dst + len);
  }

  /* We have to be more careful for the last six bytes,
   * to avoid buffer overflow in case utf8-encoded char would be too long for our dst buffer. */
  while (*src) {
    char t[6];
    size_t l = BLI_str_utf8_from_unicode((uint)*src++, t);
    BLI_assert(l <= 6);
    if (len + l > maxlen) {
      break;
    }
    memcpy(dst + len, t, l);
    len += l;
  }

  dst[len] = '\0';

  return len;
}

/* utf32 len in utf8 */
size_t BLI_str_utf32_as_utf8_len(const char32_t *src)
{
  size_t len = 0;

  while (*src) {
    len += BLI_str_utf8_from_unicode((uint)*src++, NULL);
  }

  return len;
}

/* was g_utf8_find_prev_char */
/**
 * BLI_str_find_prev_char_utf8:
 * \param str: pointer to the beginning of a UTF-8 encoded string
 * \param p: pointer to some position within \a str
 *
 * Given a position \a p with a UTF-8 encoded string \a str, find the start
 * of the previous UTF-8 character starting before. \a p Returns %NULL if no
 * UTF-8 characters are present in \a str before \a p
 *
 * \a p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 *
 * Return value: a pointer to the found character or %NULL.
 */
char *BLI_str_find_prev_char_utf8(const char *str, const char *p)
{
  for (--p; p >= str; p--) {
    if ((*p & 0xc0) != 0x80) {
      return (char *)p;
    }
  }
  return NULL;
}

/* was g_utf8_find_next_char */
/**
 * BLI_str_find_next_char_utf8:
 * \param p: a pointer to a position within a UTF-8 encoded string
 * \param end: a pointer to the byte following the end of the string,
 * or %NULL to indicate that the string is nul-terminated.
 *
 * Finds the start of the next UTF-8 character in the string after \a p
 *
 * \a p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 *
 * Return value: a pointer to the found character or %NULL
 */
char *BLI_str_find_next_char_utf8(const char *p, const char *end)
{
  if (*p) {
    if (end) {
      for (++p; p < end && (*p & 0xc0) == 0x80; p++) {
        /* do nothing */
      }
    }
    else {
      for (++p; (*p & 0xc0) == 0x80; p++) {
        /* do nothing */
      }
    }
  }
  return (p == end) ? NULL : (char *)p;
}

/* was g_utf8_prev_char */
/**
 * BLI_str_prev_char_utf8:
 * \param p: a pointer to a position within a UTF-8 encoded string
 *
 * Finds the previous UTF-8 character in the string before \a p
 *
 * \a p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte. If \a p might be the first
 * character of the string, you must use g_utf8_find_prev_char() instead.
 *
 * Return value: a pointer to the found character.
 */
char *BLI_str_prev_char_utf8(const char *p)
{
  while (1) {
    p--;
    if ((*p & 0xc0) != 0x80) {
      return (char *)p;
    }
  }
}
/* end glib copy */

size_t BLI_str_partition_utf8(const char *str,
                              const uint delim[],
                              const char **sep,
                              const char **suf)
{
  return BLI_str_partition_ex_utf8(str, NULL, delim, sep, suf, false);
}

size_t BLI_str_rpartition_utf8(const char *str,
                               const uint delim[],
                               const char **sep,
                               const char **suf)
{
  return BLI_str_partition_ex_utf8(str, NULL, delim, sep, suf, true);
}

size_t BLI_str_partition_ex_utf8(const char *str,
                                 const char *end,
                                 const uint delim[],
                                 const char **sep,
                                 const char **suf,
                                 const bool from_right)
{
  const uint *d;
  const size_t str_len = end ? (size_t)(end - str) : strlen(str);
  size_t index;

  /* Note that here, we assume end points to a valid utf8 char! */
  BLI_assert(end == NULL || (end >= str && (BLI_str_utf8_as_unicode(end) != BLI_UTF8_ERR)));

  *suf = (char *)(str + str_len);

  for (*sep = (char *)(from_right ? BLI_str_find_prev_char_utf8(str, str + str_len) : str),
      index = 0;
       *sep >= str && (!end || *sep < end) && **sep != '\0';
       *sep = (char *)(from_right ? BLI_str_find_prev_char_utf8(str, *sep) : str + index)) {
    const uint c = BLI_str_utf8_as_unicode_and_size(*sep, &index);

    if (c == BLI_UTF8_ERR) {
      *suf = *sep = NULL;
      break;
    }

    for (d = delim; *d != '\0'; d++) {
      if (*d == c) {
        /* *suf is already correct in case from_right is true. */
        if (!from_right) {
          *suf = (char *)(str + index);
        }
        return (size_t)(*sep - str);
      }
    }

    *suf = *sep; /* Useful in 'from_right' case! */
  }

  *suf = *sep = NULL;
  return str_len;
}

/* -------------------------------------------------------------------- */
/** \name Offset Conversion in Strings
 * \{ */

int BLI_str_utf8_offset_to_index(const char *str, int offset)
{
  int index = 0, pos = 0;
  while (pos != offset) {
    pos += BLI_str_utf8_size(str + pos);
    index++;
  }
  return index;
}

int BLI_str_utf8_offset_from_index(const char *str, int index)
{
  int offset = 0, pos = 0;
  while (pos != index) {
    offset += BLI_str_utf8_size(str + offset);
    pos++;
  }
  return offset;
}

int BLI_str_utf8_offset_to_column(const char *str, int offset)
{
  int column = 0, pos = 0;
  while (pos < offset) {
    column += BLI_str_utf8_char_width_safe(str + pos);
    pos += BLI_str_utf8_size_safe(str + pos);
  }
  return column;
}

int BLI_str_utf8_offset_from_column(const char *str, int column)
{
  int offset = 0, pos = 0, col;
  while (*(str + offset) && pos < column) {
    col = BLI_str_utf8_char_width_safe(str + offset);
    if (pos + col > column) {
      break;
    }
    offset += BLI_str_utf8_size_safe(str + offset);
    pos += col;
  }
  return offset;
}

/** \} */
