/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation.
 * Code from gutf8.c Copyright 1999 Tom Tromey
 *                   Copyright 2000 Red Hat, Inc.
 * All rights reserved. */

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

/**
 * Array copied from GLIB's `gutf8.c`.
 * \note last two values (0xfe and 0xff) are forbidden in UTF-8,
 * so they are considered 1 byte length too.
 */
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

ptrdiff_t BLI_str_utf8_invalid_byte(const char *str, size_t length)
{
  /* NOTE(@campbellbarton): from libswish3, originally called u8_isvalid(),
   * modified to return the index of the bad character (byte index not UTF).
   * http://svn.swish-e.org/libswish3/trunk/src/libswish3/utf8.c r3044.
   *
   * Comment from code in: `libswish3`.
   * Based on the `valid_utf8` routine from the PCRE library by Philip Hazel
   *
   * length is in bytes, since without knowing whether the string is valid
   * it's hard to know how many characters there are! */

  const uchar *p, *perr, *pend = (const uchar *)str + length;
  uchar c;
  int ab;

  for (p = (const uchar *)str; p < pend; p++, length--) {
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

int BLI_str_utf8_invalid_strip(char *str, size_t length)
{
  ptrdiff_t bad_char;
  int tot = 0;

  BLI_assert(str[length] == '\0');

  while ((bad_char = BLI_str_utf8_invalid_byte(str, length)) != -1) {
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

  /* NOTE: currently we don't attempt to deal with invalid utf8 chars. */
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

  /* NOTE: currently we don't attempt to deal with invalid utf8 chars. */
  BLI_STR_UTF8_CPY(dst, src, maxncpy);

  return (size_t)(dst - r_dst);
}

#undef BLI_STR_UTF8_CPY

/* -------------------------------------------------------------------- */
/* wchar_t / utf8 functions */

size_t BLI_strncpy_wchar_as_utf8(char *__restrict dst,
                                 const wchar_t *__restrict src,
                                 const size_t maxncpy)
{
  BLI_assert(maxncpy != 0);
  size_t len = 0;
#ifdef DEBUG_STRSIZE
  memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif
  while (*src && len < maxncpy) {
    len += BLI_str_utf8_from_unicode((uint)*src++, dst + len, maxncpy - len);
  }
  dst[len] = '\0';
  /* Return the correct length when part of the final byte did not fit into the string. */
  while ((len > 0) && UNLIKELY(dst[len - 1] == '\0')) {
    len--;
  }
  return len;
}

size_t BLI_wstrlen_utf8(const wchar_t *src)
{
  size_t len = 0;

  while (*src) {
    len += BLI_str_utf8_from_unicode_len((uint)*src++);
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
  conv_utf_8_to_16(src_c, dst_w, maxncpy);
  /* NOTE: it would be more efficient to calculate the length as part of #conv_utf_8_to_16. */
  return wcslen(dst_w);
#else
  return BLI_str_utf8_as_utf32((char32_t *)dst_w, src_c, maxncpy);
#endif
}

/* end wchar_t / utf8 functions */
/* -------------------------------------------------------------------- */

int BLI_wcwidth(char32_t ucs)
{
  /* Treat private use areas (icon fonts), symbols, and emoticons as double-width. */
  if (ucs >= 0xf0000 || (ucs >= 0xe000 && ucs < 0xf8ff) || (ucs >= 0x1f300 && ucs < 0x1fbff)) {
    return 2;
  }
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

/* -------------------------------------------------------------------- */

/* copied from glib's gutf8.c, added 'Err' arg */

/* NOTE(@campbellbarton): glib uses uint for unicode, best we do the same,
 * though we don't typedef it. */

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

int BLI_str_utf8_size(const char *p)
{
  /* NOTE: uses glib functions but not from GLIB. */

  int mask = 0, len;
  const uchar c = (uchar)*p;

  UTF8_COMPUTE(c, mask, len, -1);

  (void)mask; /* quiet warning */

  return len;
}

int BLI_str_utf8_size_safe(const char *p)
{
  int mask = 0, len;
  const uchar c = (uchar)*p;

  UTF8_COMPUTE(c, mask, len, 1);

  (void)mask; /* quiet warning */

  return len;
}

uint BLI_str_utf8_as_unicode(const char *p)
{
  /* Originally `g_utf8_get_char` in GLIB. */

  int i, len;
  uint mask = 0;
  uint result;
  const uchar c = (uchar)*p;

  UTF8_COMPUTE(c, mask, len, -1);
  if (UNLIKELY(len == -1)) {
    return BLI_UTF8_ERR;
  }
  UTF8_GET(result, p, i, mask, len, BLI_UTF8_ERR);

  return result;
}

uint BLI_str_utf8_as_unicode_step_or_error(const char *__restrict p,
                                           const size_t p_len,
                                           size_t *__restrict index)
{
  int i, len;
  uint mask = 0;
  uint result;
  const uchar c = (uchar) * (p += *index);

  BLI_assert(*index < p_len);
  BLI_assert(c != '\0');

  UTF8_COMPUTE(c, mask, len, -1);
  if (UNLIKELY(len == -1) || (*index + (size_t)len > p_len)) {
    return BLI_UTF8_ERR;
  }
  UTF8_GET(result, p, i, mask, len, BLI_UTF8_ERR);
  if (UNLIKELY(result == BLI_UTF8_ERR)) {
    return BLI_UTF8_ERR;
  }
  *index += (size_t)len;
  BLI_assert(*index <= p_len);
  return result;
}

uint BLI_str_utf8_as_unicode_step(const char *__restrict p,
                                  const size_t p_len,
                                  size_t *__restrict index)
{
  uint result = BLI_str_utf8_as_unicode_step_or_error(p, p_len, index);
  if (UNLIKELY(result == BLI_UTF8_ERR)) {
    result = (uint)p[*index];
    *index += 1;
  }
  BLI_assert(*index <= p_len);
  return result;
}

/* was g_unichar_to_utf8 */

#define UTF8_VARS_FROM_CHAR32(Char, First, Len) \
  if (Char < 0x80) { \
    First = 0; \
    Len = 1; \
  } \
  else if (Char < 0x800) { \
    First = 0xc0; \
    Len = 2; \
  } \
  else if (Char < 0x10000) { \
    First = 0xe0; \
    Len = 3; \
  } \
  else if (Char < 0x200000) { \
    First = 0xf0; \
    Len = 4; \
  } \
  else if (Char < 0x4000000) { \
    First = 0xf8; \
    Len = 5; \
  } \
  else { \
    First = 0xfc; \
    Len = 6; \
  } \
  (void)0

size_t BLI_str_utf8_from_unicode_len(const uint c)
{
  /* If this gets modified, also update the copy in g_string_insert_unichar() */
  uint len = 0;
  uint first;

  UTF8_VARS_FROM_CHAR32(c, first, len);
  (void)first;

  return len;
}

size_t BLI_str_utf8_from_unicode(uint c, char *outbuf, const size_t outbuf_len)

{
  /* If this gets modified, also update the copy in g_string_insert_unichar() */
  uint len = 0;
  uint first;

  UTF8_VARS_FROM_CHAR32(c, first, len);

  if (UNLIKELY(outbuf_len < len)) {
    /* NULL terminate instead of writing a partial byte. */
    memset(outbuf, 0x0, outbuf_len);
    return outbuf_len;
  }

  for (uint i = len - 1; i > 0; i--) {
    outbuf[i] = (c & 0x3f) | 0x80;
    c >>= 6;
  }
  outbuf[0] = c | first;

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

  const size_t src_c_len = strlen(src_c);
  const char *src_c_end = src_c + src_c_len;
  size_t index = 0;
  while ((index < src_c_len) && (len != maxlen)) {
    const uint unicode = BLI_str_utf8_as_unicode_step_or_error(src_c, src_c_len, &index);
    if (unicode != BLI_UTF8_ERR) {
      *dst_w = unicode;
    }
    else {
      *dst_w = '?';
      const char *src_c_next = BLI_str_find_next_char_utf8(src_c + index, src_c_end);
      index = (size_t)(src_c_next - src_c);
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
  BLI_assert(maxncpy != 0);
  size_t len = 0;
#ifdef DEBUG_STRSIZE
  memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif
  while (*src && len < maxncpy) {
    len += BLI_str_utf8_from_unicode((uint)*src++, dst + len, maxncpy - len);
  }
  dst[len] = '\0';
  /* Return the correct length when part of the final byte did not fit into the string. */
  while ((len > 0) && UNLIKELY(dst[len - 1] == '\0')) {
    len--;
  }
  return len;
}

size_t BLI_str_utf32_as_utf8_len(const char32_t *src)
{
  size_t len = 0;

  while (*src) {
    len += BLI_str_utf8_from_unicode_len((uint)*src++);
  }

  return len;
}

const char *BLI_str_find_prev_char_utf8(const char *p, const char *str_start)
{
  /* Originally `g_utf8_find_prev_char` in GLIB. */

  BLI_assert(p >= str_start);
  if (str_start < p) {
    for (--p; p >= str_start; p--) {
      if ((*p & 0xc0) != 0x80) {
        return (char *)p;
      }
    }
  }
  return p;
}

const char *BLI_str_find_next_char_utf8(const char *p, const char *str_end)
{
  /* Originally `g_utf8_find_next_char` in GLIB. */

  BLI_assert(p <= str_end);
  if ((p < str_end) && (*p != '\0')) {
    for (++p; p < str_end && (*p & 0xc0) == 0x80; p++) {
      /* do nothing */
    }
  }
  return p;
}

size_t BLI_str_partition_utf8(const char *str,
                              const uint delim[],
                              const char **r_sep,
                              const char **r_suf)
{
  return BLI_str_partition_ex_utf8(str, NULL, delim, r_sep, r_suf, false);
}

size_t BLI_str_rpartition_utf8(const char *str,
                               const uint delim[],
                               const char **r_sep,
                               const char **r_suf)
{
  return BLI_str_partition_ex_utf8(str, NULL, delim, r_sep, r_suf, true);
}

size_t BLI_str_partition_ex_utf8(const char *str,
                                 const char *end,
                                 const uint delim[],
                                 const char **r_sep,
                                 const char **r_suf,
                                 const bool from_right)
{
  const size_t str_len = end ? (size_t)(end - str) : strlen(str);
  if (end == NULL) {
    end = str + str_len;
  }

  /* Note that here, we assume end points to a valid utf8 char! */
  BLI_assert((end >= str) && (BLI_str_utf8_as_unicode(end) != BLI_UTF8_ERR));

  char *suf = (char *)(str + str_len);
  size_t index = 0;
  for (char *sep = (char *)(from_right ? BLI_str_find_prev_char_utf8(end, str) : str);
       from_right ? (sep > str) : ((sep < end) && (*sep != '\0'));
       sep = (char *)(from_right ? (str != sep ? BLI_str_find_prev_char_utf8(sep, str) : NULL) :
                                   str + index)) {
    size_t index_ofs = 0;
    const uint c = BLI_str_utf8_as_unicode_step_or_error(sep, (size_t)(end - sep), &index_ofs);
    if (UNLIKELY(c == BLI_UTF8_ERR)) {
      break;
    }
    index += index_ofs;

    for (const uint *d = delim; *d != '\0'; d++) {
      if (*d == c) {
        /* `suf` is already correct in case from_right is true. */
        *r_sep = sep;
        *r_suf = from_right ? suf : (char *)(str + index);
        return (size_t)(sep - str);
      }
    }

    suf = sep; /* Useful in 'from_right' case! */
  }

  *r_suf = *r_sep = NULL;
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
  int offset = 0, pos = 0;
  while (*(str + offset) && pos < column) {
    const int col = BLI_str_utf8_char_width_safe(str + offset);
    if (pos + col > column) {
      break;
    }
    offset += BLI_str_utf8_size_safe(str + offset);
    pos += col;
  }
  return offset;
}

/** \} */
