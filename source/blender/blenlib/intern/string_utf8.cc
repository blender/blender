/* SPDX-FileCopyrightText: 1999 Tom Tromey
 * SPDX-FileCopyrightText: 2000 Red Hat, Inc. All rights reserved.
 * SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Code from `gutf8.c` by Tom Tromey & Red Hat, Inc. */

/** \file
 * \ingroup bli
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <wcwidth.h>

#include "BLI_utildefines.h"

#include "BLI_string.h"      /* #BLI_string_debug_size. */
#include "BLI_string_utf8.h" /* own include */
#ifdef WIN32
#  include "utfconv.hh"
#endif
#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

static size_t str_utf8_truncate_at_size_unchecked(char *str, const size_t str_size);

/* -------------------------------------------------------------------- */
/** \name UTF8 Character Decoding (Skip & Mask Lookup)
 *
 * Derived from GLIB `gutf8.c`.
 *
 * Ranges (zero based, inclusive):
 *
 * - 000..127: 1 byte.
 * - 128..191: invalid.
 * - 192..223: 2 bytes.
 * - 224..239: 3 bytes.
 * - 240..247: 4 bytes.
 * - 248..251: 4 bytes.
 * - 252..253: 4 bytes.
 * - 254..255: invalid.
 *
 * Invalid values fall back to 1 byte or -1 (for an error value).
 *
 * \note From testing string copying via #BLI_strncpy_utf8 with large (multi-megabyte) strings,
 * using a function instead of a lookup-table is between 2 & 3 times faster.
 * \{ */

BLI_INLINE int utf8_char_compute_skip(const char c)
{
  if (UNLIKELY(c >= 192)) {
    if ((c & 0xe0) == 0xc0) {
      return 2;
    }
    if ((c & 0xf0) == 0xe0) {
      return 3;
    }
    if ((c & 0xf8) == 0xf0) {
      return 4;
    }
    if ((c & 0xfc) == 0xf8) {
      return 5;
    }
    if ((c & 0xfe) == 0xfc) {
      return 6;
    }
  }
  return 1;
}

BLI_INLINE int utf8_char_compute_skip_or_error(const char c)
{
  if (c < 128) {
    return 1;
  }
  if ((c & 0xe0) == 0xc0) {
    return 2;
  }
  if ((c & 0xf0) == 0xe0) {
    return 3;
  }
  if ((c & 0xf8) == 0xf0) {
    return 4;
  }
  if ((c & 0xfc) == 0xf8) {
    return 5;
  }
  if ((c & 0xfe) == 0xfc) {
    return 6;
  }
  return -1;
}

BLI_INLINE int utf8_char_compute_skip_or_error_with_mask(const char c, char *r_mask)
{
  /* Originally from GLIB `UTF8_COMPUTE` macro. */
  if (c < 128) {
    *r_mask = 0x7f;
    return 1;
  }
  if ((c & 0xe0) == 0xc0) {
    *r_mask = 0x1f;
    return 2;
  }
  if ((c & 0xf0) == 0xe0) {
    *r_mask = 0x0f;
    return 3;
  }
  if ((c & 0xf8) == 0xf0) {
    *r_mask = 0x07;
    return 4;
  }
  if ((c & 0xfc) == 0xf8) {
    *r_mask = 0x03;
    return 5;
  }
  if ((c & 0xfe) == 0xfc) {
    *r_mask = 0x01;
    return 6;
  }
  return -1;
}

/**
 * Decode a UTF8 code-point, use in combination with #utf8_char_compute_skip_or_error_with_mask.
 */
BLI_INLINE uint utf8_char_decode(const char *p, const char mask, const int len, const uint err)
{
  /* Originally from GLIB `UTF8_GET` macro, added an 'err' argument. */
  uint result = p[0] & mask;
  for (int count = 1; count < len; count++) {
    if ((p[count] & 0xc0) != 0x80) {
      return err;
    }
    result <<= 6;
    result |= p[count] & 0x3f;
  }
  return result;
}

/** \} */

ptrdiff_t BLI_str_utf8_invalid_byte(const char *str, size_t str_len)
{
  /* NOTE(@ideasman42): from libswish3, originally called `u8_isvalid()`,
   * modified to return the index of the bad character (byte index not UTF).
   * http://svn.swish-e.org/libswish3/trunk/src/libswish3/utf8.c r3044.
   *
   * Comment from code in: `libswish3`.
   * Based on the `valid_utf8` routine from the PCRE library by Philip Hazel
   *
   * length is in bytes, since without knowing whether the string is valid
   * it's hard to know how many characters there are! */

  const uchar *p, *perr, *pend = (const uchar *)str + str_len;
  uchar c;
  int ab;

  for (p = (const uchar *)str; p < pend; p++, str_len--) {
    c = *p;
    perr = p; /* Erroneous char is always the first of an invalid UTF8 sequence... */
    if (ELEM(c, 0xfe, 0xff, 0x00)) {
      /* Those three values are not allowed in UTF8 string. */
      goto utf8_error;
    }
    if (c < 128) {
      continue;
    }
    if ((c & 0xc0) != 0xc0) {
      goto utf8_error;
    }

    /* Note that since we always increase p (and decrease length) by one byte in main loop,
     * we only add/subtract extra UTF8 bytes in code below
     * (ab number, aka number of bytes remaining in the UTF8 sequence after the initial one). */
    ab = utf8_char_compute_skip(c) - 1;
    if (str_len <= size_t(ab)) {
      goto utf8_error;
    }

    /* Check top bits in the second byte */
    p++;
    str_len--;
    if ((*p & 0xc0) != 0x80) {
      goto utf8_error;
    }

    /* Check for overlong sequences for each different length */
    switch (ab) {
      case 1:
        /* Check for: `XX00 000X`. */
        if ((c & 0x3e) == 0) {
          goto utf8_error;
        }
        continue; /* We know there aren't any more bytes to check */

      case 2:
        /* Check for: `1110 0000, XX0X XXXX`. */
        if (c == 0xe0 && (*p & 0x20) == 0) {
          goto utf8_error;
        }
        /* Some special cases, see section 5 of UTF8 decoder stress-test by Markus Kuhn
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
        /* Check for: `1111 0000, XX00 XXXX`. */
        if (c == 0xf0 && (*p & 0x30) == 0) {
          goto utf8_error;
        }
        break;

      case 4:
        /* Check for `1111 1000, XX00 0XXX`. */
        if (c == 0xf8 && (*p & 0x38) == 0) {
          goto utf8_error;
        }
        break;

      case 5:
        /* Check for: `1111 1100, XX00 00XX`. */
        if (c == 0xfc && (*p & 0x3c) == 0) {
          goto utf8_error;
        }
        break;
    }

    /* Check for valid bytes after the 2nd, if any; all must start 10. */
    while (--ab > 0) {
      p++;
      str_len--;
      if ((*p & 0xc0) != 0x80) {
        goto utf8_error;
      }
    }
  }

  return -1;

utf8_error:

  return ((const char *)perr - (const char *)str);
}

int BLI_str_utf8_invalid_strip(char *str, size_t str_len)
{
  ptrdiff_t bad_char;
  int tot = 0;

  BLI_assert(str[str_len] == '\0');

  while ((bad_char = BLI_str_utf8_invalid_byte(str, str_len)) != -1) {
    str += bad_char;
    str_len -= size_t(bad_char + 1);

    if (str_len == 0) {
      /* last character bad, strip it */
      *str = '\0';
      tot++;
      break;
    }
    /* strip, keep looking */
    memmove(str, str + 1, str_len + 1); /* +1 for null char! */
    tot++;
  }

  return tot;
}

int BLI_str_utf8_invalid_substitute(char *str, size_t str_len, const char substitute)
{
  BLI_assert(substitute);
  ptrdiff_t bad_char;
  int tot = 0;

  BLI_assert(str[str_len] == '\0');

  while ((bad_char = BLI_str_utf8_invalid_byte(str, str_len)) != -1) {
    str[bad_char] = substitute;
    bad_char += 1; /* Step over the bad character. */
    str += bad_char;
    str_len -= size_t(bad_char);
    tot++;
  }

  return tot;
}

const char *BLI_str_utf8_invalid_substitute_if_needed(const char *str,
                                                      const size_t str_len,
                                                      const char substitute,
                                                      char *buf,
                                                      const size_t buf_maxncpy)
{
  BLI_assert(str[str_len] == '\0');
  const ptrdiff_t bad_char = BLI_str_utf8_invalid_byte(str, str_len);
  if (LIKELY(bad_char == -1)) {
    return str;
  }
  BLI_assert(bad_char >= 0);

  /* In the case a bad character is outside the buffer limit,
   * simply perform a truncating UTF8 copy into the buffer and return that. */
  if (UNLIKELY(size_t(bad_char) >= buf_maxncpy)) {
    BLI_strncpy_utf8(buf, str, buf_maxncpy);
    return buf;
  }

  size_t buf_len;
  if (str_len < buf_maxncpy) {
    memcpy(buf, str, str_len + 1);
    buf_len = str_len;
  }
  else {
    buf_len = BLI_strncpy_rlen(buf, str, buf_maxncpy);
  }

  /* Skip the good characters. */
  BLI_str_utf8_invalid_substitute(buf + bad_char, buf_len - size_t(bad_char), substitute);
  return buf;
}

/**
 * Internal utility for implementing #BLI_strncpy_utf8 / #BLI_strncpy_utf8_rlen.
 *
 * Compatible with #BLI_strncpy, but ensure no partial UTF8 chars.
 *
 * \param dst_maxncpy: The maximum number of bytes to copy. This does not include the null
 *   terminator.
 *
 * \note currently we don't attempt to deal with invalid UTF8 chars.
 * See #BLI_str_utf8_invalid_strip for if that is needed.
 *
 * \note the caller is responsible for null terminating the string.
 */
BLI_INLINE char *str_utf8_copy_max_bytes_impl(char *dst, const char *src, size_t dst_maxncpy)
{
  /* Cast to `uint8_t` is a no-op, quiets array subscript of type `char` warning.
   * No need to check `src` points to a nil byte as this will return from the switch statement. */
  size_t utf8_size;
  while ((utf8_size = size_t(utf8_char_compute_skip(*src))) <= dst_maxncpy) {
    dst_maxncpy -= utf8_size;
    /* Prefer more compact block. */
    /* NOLINTBEGIN: bugprone-assignment-in-if-condition */
    /* clang-format off */
    switch (utf8_size) {
      case 6: if (UNLIKELY(!(*dst = *src++))) { return dst; } dst++; ATTR_FALLTHROUGH;
      case 5: if (UNLIKELY(!(*dst = *src++))) { return dst; } dst++; ATTR_FALLTHROUGH;
      case 4: if (UNLIKELY(!(*dst = *src++))) { return dst; } dst++; ATTR_FALLTHROUGH;
      case 3: if (UNLIKELY(!(*dst = *src++))) { return dst; } dst++; ATTR_FALLTHROUGH;
      case 2: if (UNLIKELY(!(*dst = *src++))) { return dst; } dst++; ATTR_FALLTHROUGH;
      case 1: if (UNLIKELY(!(*dst = *src++))) { return dst; } dst++;
    }
    /* clang-format on */
    /* NOLINTEND: bugprone-assignment-in-if-condition */
  }
  return dst;
}

char *BLI_strncpy_utf8(char *__restrict dst, const char *__restrict src, size_t dst_maxncpy)
{
  BLI_assert(dst_maxncpy != 0);
  BLI_string_debug_size(dst, dst_maxncpy);

  char *dst_end = str_utf8_copy_max_bytes_impl(dst, src, dst_maxncpy - 1);
  *dst_end = '\0';
  return dst;
}

size_t BLI_strncpy_utf8_rlen(char *__restrict dst, const char *__restrict src, size_t dst_maxncpy)
{
  BLI_assert(dst_maxncpy != 0);
  BLI_string_debug_size(dst, dst_maxncpy);

  char *r_dst = dst;
  dst = str_utf8_copy_max_bytes_impl(dst, src, dst_maxncpy - 1);
  *dst = '\0';

  return size_t(dst - r_dst);
}

size_t BLI_strncpy_utf8_rlen_unterminated(char *__restrict dst,
                                          const char *__restrict src,
                                          size_t dst_maxncpy)
{
  BLI_string_debug_size(dst, dst_maxncpy);

  char *r_dst = dst;
  dst = str_utf8_copy_max_bytes_impl(dst, src, dst_maxncpy);

  return size_t(dst - r_dst);
}

/* -------------------------------------------------------------------- */
/* wchar_t / UTF8 functions */

size_t BLI_strncpy_wchar_as_utf8(char *__restrict dst,
                                 const wchar_t *__restrict src,
                                 const size_t dst_maxncpy)
{
  BLI_assert(dst_maxncpy != 0);
  BLI_string_debug_size(dst, dst_maxncpy);

  size_t len = 0;
  while (*src && len < dst_maxncpy) {
    len += BLI_str_utf8_from_unicode(uint(*src++), dst + len, dst_maxncpy - len);
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
    len += BLI_str_utf8_from_unicode_len(uint(*src++));
  }

  return len;
}

size_t BLI_strlen_utf8_ex(const char *strc, size_t *r_len_bytes)
{
  size_t len = 0;
  const char *strc_orig = strc;

  while (*strc) {
    int step = BLI_str_utf8_size_safe(strc);

    /* Detect null bytes within multi-byte sequences.
     * This matches the behavior of #BLI_strncpy_utf8 for incomplete byte sequences. */
    for (int i = 1; i < step; i++) {
      if (UNLIKELY(strc[i] == '\0')) {
        step = i;
        break;
      }
    }

    strc += step;
    len++;
  }

  *r_len_bytes = size_t(strc - strc_orig);
  return len;
}

size_t BLI_strlen_utf8(const char *strc)
{
  size_t len_bytes;
  return BLI_strlen_utf8_ex(strc, &len_bytes);
}

size_t BLI_strnlen_utf8_ex(const char *strc, const size_t strc_maxlen, size_t *r_len_bytes)
{
  size_t len = 0;
  const char *strc_orig = strc;
  const char *strc_end = strc + strc_maxlen;

  while (*strc) {
    int step = BLI_str_utf8_size_safe(strc);
    if (strc + step > strc_end) {
      break;
    }

    /* Detect null bytes within multi-byte sequences.
     * This matches the behavior of #BLI_strncpy_utf8 for incomplete byte sequences. */
    for (int i = 1; i < step; i++) {
      if (UNLIKELY(strc[i] == '\0')) {
        step = i;
        break;
      }
    }
    strc += step;
    len++;
  }

  *r_len_bytes = size_t(strc - strc_orig);
  return len;
}

size_t BLI_strnlen_utf8(const char *strc, const size_t strc_maxlen)
{
  size_t len_bytes;
  return BLI_strnlen_utf8_ex(strc, strc_maxlen, &len_bytes);
}

size_t BLI_strncpy_wchar_from_utf8(wchar_t *__restrict dst_w,
                                   const char *__restrict src_c,
                                   const size_t dst_w_maxncpy)
{
#ifdef WIN32
  BLI_string_debug_size(dst_w, dst_w_maxncpy);
  conv_utf_8_to_16(src_c, dst_w, dst_w_maxncpy);
  /* NOTE: it would be more efficient to calculate the length as part of #conv_utf_8_to_16. */
  return wcslen(dst_w);
#else
  return BLI_str_utf8_as_utf32((char32_t *)dst_w, src_c, dst_w_maxncpy);
#endif
}

/* End wchar_t / UTF8 functions. */
/* -------------------------------------------------------------------- */

size_t BLI_vsnprintf_utf8(char *__restrict dst,
                          size_t dst_maxncpy,
                          const char *__restrict format,
                          va_list arg)
{
  /* NOTE: a clone of #BLI_vsnprintf that trims the end. */
  BLI_string_debug_size(dst, dst_maxncpy);

  BLI_assert(dst != nullptr);
  BLI_assert(dst_maxncpy > 0);
  BLI_assert(format != nullptr);

  const size_t n = size_t(vsnprintf(dst, dst_maxncpy, format, arg));
  if (n < dst_maxncpy) {
    dst[n] = '\0';
  }
  else {
    str_utf8_truncate_at_size_unchecked(dst, dst_maxncpy);
  }

  return n;
}

size_t BLI_vsnprintf_utf8_rlen(char *__restrict dst,
                               size_t dst_maxncpy,
                               const char *__restrict format,
                               va_list arg)
{
  BLI_string_debug_size(dst, dst_maxncpy);

  BLI_assert(dst != nullptr);
  BLI_assert(dst_maxncpy > 0);
  BLI_assert(format != nullptr);

  size_t n = size_t(vsnprintf(dst, dst_maxncpy, format, arg));
  if (n < dst_maxncpy) {
    dst[n] = '\0';
  }
  else {
    n = str_utf8_truncate_at_size_unchecked(dst, dst_maxncpy);
  }
  return n;
}

size_t BLI_snprintf_utf8(char *__restrict dst,
                         size_t dst_maxncpy,
                         const char *__restrict format,
                         ...)
{
  BLI_string_debug_size(dst, dst_maxncpy);

  va_list arg;
  va_start(arg, format);
  const size_t n = BLI_vsnprintf_utf8(dst, dst_maxncpy, format, arg);
  va_end(arg);

  return n;
}

size_t BLI_snprintf_utf8_rlen(char *__restrict dst,
                              size_t dst_maxncpy,
                              const char *__restrict format,
                              ...)
{
  BLI_string_debug_size(dst, dst_maxncpy);

  va_list arg;
  va_start(arg, format);
  const size_t n = BLI_vsnprintf_utf8_rlen(dst, dst_maxncpy, format, arg);
  va_end(arg);

  return n;
}

int BLI_wcwidth_or_error(char32_t ucs)
{
  /* Treat private use areas (icon fonts), symbols, and emoticons as double-width. */
  if (ucs >= 0xf0000 || (ucs >= 0xe000 && ucs < 0xf8ff) || (ucs >= 0x1f300 && ucs < 0x1fbff)) {
    return 2;
  }
  return mk_wcwidth(ucs);
}

int BLI_wcwidth_safe(char32_t ucs)
{
  const int columns = BLI_wcwidth_or_error(ucs);
  if (columns >= 0) {
    return columns;
  }
  return 1;
}

int BLI_wcswidth_or_error(const char32_t *pwcs, size_t n)
{
  return mk_wcswidth(pwcs, n);
}

int BLI_str_utf8_char_width_or_error(const char *p)
{
  uint unicode = BLI_str_utf8_as_unicode_or_error(p);
  if (unicode == BLI_UTF8_ERR) {
    return -1;
  }

  return BLI_wcwidth_or_error(char32_t(unicode));
}

int BLI_str_utf8_char_width_safe(const char *p)
{
  uint unicode = BLI_str_utf8_as_unicode_or_error(p);
  if (unicode == BLI_UTF8_ERR) {
    return 1;
  }

  return BLI_wcwidth_safe(char32_t(unicode));
}

/* -------------------------------------------------------------------- */
/** \name UTF32 Case Conversion
 *
 * \warning the lower/uppercase form of some characters use multiple characters.
 * These cases are not accounted for by this conversion function.
 * A common example is the German `eszett` / `scharfes`.
 * Supporting such cases would have to operate on a character array, with support for resizing.
 * (for reference - Python's upper/lower functions support this).
 * \{ */

char32_t BLI_str_utf32_char_to_upper(const char32_t wc)
{
  if (wc < U'\xFF') { /* Latin. */
    if ((wc <= U'z' && wc >= U'a') || (wc <= U'\xF6' && wc >= U'\xE0') ||
        /* Correct but the first case is know, only check the second */
        // (wc <= U'\xFE' && wc >= U'\xF8')
        (wc >= U'\xF8'))
    {
      return wc - 32;
    }
    return wc;
  }

  if ((wc <= U'\x137' && wc >= U'\x101') || (wc <= U'\x1E95' && wc >= U'\x1E01')) {
    /* Latin Extended. */
    return (wc & 1) ? wc - 1 : wc;
  }
  if ((wc <= U'\x586' && wc >= U'\x561') || (wc <= U'\x10F5' && wc >= U'\x10D0')) {
    /* Armenian and Georgian */
    return wc - 48;
  }
  if (wc <= U'\x24E9' && wc >= U'\x24D0') { /* Enclosed Numerals. */
    return wc - 26;
  }
  if (wc <= U'\xFF5A' && wc >= U'\xFF41') { /* Full-width Forms. */
    return wc - 32;
  }

  /* There are only three remaining ranges that contain capitalization. */
  if (!(wc <= U'\x0292' && wc >= U'\x00FF') && !(wc <= U'\x04F9' && wc >= U'\x03AC') &&
      !(wc <= U'\x1FE1' && wc >= U'\x1E01'))
  {
    return wc;
  }

  static const char32_t from[] =
      U"\x00FF\x013A\x013C\x013E\x0140\x0142\x0144\x0146\x0148\x014B\x014D\x014F\x0151\x0153\x0155"
      U"\x0157\x0159\x015B\x015D\x015F\x0161\x0163\x0165\x0167\x0169\x016B\x016D\x016F\x0171\x0173"
      U"\x0175\x0177\x017A\x017C\x017E\x0183\x0185\x0188\x018C\x0192\x0199\x01A1\x01A3\x01A5\x01A8"
      U"\x01AD\x01B0\x01B4\x01B6\x01B9\x01BD\x01C6\x01C9\x01CC\x01CE\x01D0\x01D2\x01D4\x01D6\x01D8"
      U"\x01DA\x01DC\x01DF\x01E1\x01E3\x01E5\x01E7\x01E9\x01EB\x01ED\x01EF\x01F3\x01F5\x01FB\x01FD"
      U"\x01FF\x0201\x0203\x0205\x0207\x0209\x020B\x020D\x020F\x0211\x0213\x0215\x0217\x0253\x0254"
      U"\x0257\x0258\x0259\x025B\x0260\x0263\x0268\x0269\x026F\x0272\x0275\x0283\x0288\x028A\x028B"
      U"\x0292\x03AC\x03AD\x03AE\x03AF\x03B1\x03B2\x03B3\x03B4\x03B5\x03B6\x03B7\x03B8\x03B9\x03BA"
      U"\x03BB\x03BC\x03BD\x03BE\x03BF\x03C0\x03C1\x03C3\x03C4\x03C5\x03C6\x03C7\x03C8\x03C9\x03CA"
      U"\x03CB\x03CC\x03CD\x03CE\x03E3\x03E5\x03E7\x03E9\x03EB\x03ED\x03EF\x0430\x0431\x0432\x0433"
      U"\x0434\x0435\x0436\x0437\x0438\x0439\x043A\x043B\x043C\x043D\x043E\x043F\x0440\x0441\x0442"
      U"\x0443\x0444\x0445\x0446\x0447\x0448\x0449\x044A\x044B\x044C\x044D\x044E\x044F\x0451\x0452"
      U"\x0453\x0454\x0455\x0456\x0457\x0458\x0459\x045A\x045B\x045C\x045E\x045F\x0461\x0463\x0465"
      U"\x0467\x0469\x046B\x046D\x046F\x0471\x0473\x0475\x0477\x0479\x047B\x047D\x047F\x0481\x0491"
      U"\x0493\x0495\x0497\x0499\x049B\x049D\x049F\x04A1\x04A3\x04A5\x04A7\x04A9\x04AB\x04AD\x04AF"
      U"\x04B1\x04B3\x04B5\x04B7\x04B9\x04BB\x04BD\x04BF\x04C2\x04C4\x04C8\x04CC\x04D1\x04D3\x04D5"
      U"\x04D7\x04D9\x04DB\x04DD\x04DF\x04E1\x04E3\x04E5\x04E7\x04E9\x04EB\x04EF\x04F1\x04F3\x04F5"
      U"\x04F9\x1EA1\x1EA3\x1EA5\x1EA7\x1EA9\x1EAB\x1EAD\x1EAF\x1EB1\x1EB3\x1EB5\x1EB7\x1EB9\x1EBB"
      U"\x1EBD\x1EBF\x1EC1\x1EC3\x1EC5\x1EC7\x1EC9\x1ECB\x1ECD\x1ECF\x1ED1\x1ED3\x1ED5\x1ED7\x1ED9"
      U"\x1EDB\x1EDD\x1EDF\x1EE1\x1EE3\x1EE5\x1EE7\x1EE9\x1EEB\x1EED\x1EEF\x1EF1\x1EF3\x1EF5\x1EF7"
      U"\x1EF9\x1F00\x1F01\x1F02\x1F03\x1F04\x1F05\x1F06\x1F07\x1F10\x1F11\x1F12\x1F13\x1F14\x1F15"
      U"\x1F20\x1F21\x1F22\x1F23\x1F24\x1F25\x1F26\x1F27\x1F30\x1F31\x1F32\x1F33\x1F34\x1F35\x1F36"
      U"\x1F37\x1F40\x1F41\x1F42\x1F43\x1F44\x1F45\x1F51\x1F53\x1F55\x1F57\x1F60\x1F61\x1F62\x1F63"
      U"\x1F64\x1F65\x1F66\x1F67\x1F80\x1F81\x1F82\x1F83\x1F84\x1F85\x1F86\x1F87\x1F90\x1F91\x1F92"
      U"\x1F93\x1F94\x1F95\x1F96\x1F97\x1FA0\x1FA1\x1FA2\x1FA3\x1FA4\x1FA5\x1FA6\x1FA7\x1FB0\x1FB1"
      U"\x1FD0\x1FD1\x1FE0\x1FE1";
  static const char32_t to[] =
      U"\x0178\x0139\x013B\x013D\x013F\x0141\x0143\x0145\x0147\x014A\x014C\x014E\x0150\x0152\x0154"
      U"\x0156\x0158\x015A\x015C\x015E\x0160\x0162\x0164\x0166\x0168\x016A\x016C\x016E\x0170\x0172"
      U"\x0174\x0176\x0179\x017B\x017D\x0182\x0184\x0187\x018B\x0191\x0198\x01A0\x01A2\x01A4\x01A7"
      U"\x01AC\x01AF\x01B3\x01B5\x01B8\x01BC\x01C4\x01C7\x01CA\x01CD\x01CF\x01D1\x01D3\x01D5\x01D7"
      U"\x01D9\x01DB\x01DE\x01E0\x01E2\x01E4\x01E6\x01E8\x01EA\x01EC\x01EE\x01F1\x01F4\x01FA\x01FC"
      U"\x01FE\x0200\x0202\x0204\x0206\x0208\x020A\x020C\x020E\x0210\x0212\x0214\x0216\x0181\x0186"
      U"\x018A\x018E\x018F\x0190\x0193\x0194\x0197\x0196\x019C\x019D\x019F\x01A9\x01AE\x01B1\x01B2"
      U"\x01B7\x0386\x0388\x0389\x038A\x0391\x0392\x0393\x0394\x0395\x0396\x0397\x0398\x0399\x039A"
      U"\x039B\x039C\x039D\x039E\x039F\x03A0\x03A1\x03A3\x03A4\x03A5\x03A6\x03A7\x03A8\x03A9\x03AA"
      U"\x03AB\x038C\x038E\x038F\x03E2\x03E4\x03E6\x03E8\x03EA\x03EC\x03EE\x0410\x0411\x0412\x0413"
      U"\x0414\x0415\x0416\x0417\x0418\x0419\x041A\x041B\x041C\x041D\x041E\x041F\x0420\x0421\x0422"
      U"\x0423\x0424\x0425\x0426\x0427\x0428\x0429\x042A\x042B\x042C\x042D\x042E\x042F\x0401\x0402"
      U"\x0403\x0404\x0405\x0406\x0407\x0408\x0409\x040A\x040B\x040C\x040E\x040F\x0460\x0462\x0464"
      U"\x0466\x0468\x046A\x046C\x046E\x0470\x0472\x0474\x0476\x0478\x047A\x047C\x047E\x0480\x0490"
      U"\x0492\x0494\x0496\x0498\x049A\x049C\x049E\x04A0\x04A2\x04A4\x04A6\x04A8\x04AA\x04AC\x04AE"
      U"\x04B0\x04B2\x04B4\x04B6\x04B8\x04BA\x04BC\x04BE\x04C1\x04C3\x04C7\x04CB\x04D0\x04D2\x04D4"
      U"\x04D6\x04D8\x04DA\x04DC\x04DE\x04E0\x04E2\x04E4\x04E6\x04E8\x04EA\x04EE\x04F0\x04F2\x04F4"
      U"\x04F8\x1EA0\x1EA2\x1EA4\x1EA6\x1EA8\x1EAA\x1EAC\x1EAE\x1EB0\x1EB2\x1EB4\x1EB6\x1EB8\x1EBA"
      U"\x1EBC\x1EBE\x1EC0\x1EC2\x1EC4\x1EC6\x1EC8\x1ECA\x1ECC\x1ECE\x1ED0\x1ED2\x1ED4\x1ED6\x1ED8"
      U"\x1EDA\x1EDC\x1EDE\x1EE0\x1EE2\x1EE4\x1EE6\x1EE8\x1EEA\x1EEC\x1EEE\x1EF0\x1EF2\x1EF4\x1EF6"
      U"\x1EF8\x1F08\x1F09\x1F0A\x1F0B\x1F0C\x1F0D\x1F0E\x1F0F\x1F18\x1F19\x1F1A\x1F1B\x1F1C\x1F1D"
      U"\x1F28\x1F29\x1F2A\x1F2B\x1F2C\x1F2D\x1F2E\x1F2F\x1F38\x1F39\x1F3A\x1F3B\x1F3C\x1F3D\x1F3E"
      U"\x1F3F\x1F48\x1F49\x1F4A\x1F4B\x1F4C\x1F4D\x1F59\x1F5B\x1F5D\x1F5F\x1F68\x1F69\x1F6A\x1F6B"
      U"\x1F6C\x1F6D\x1F6E\x1F6F\x1F88\x1F89\x1F8A\x1F8B\x1F8C\x1F8D\x1F8E\x1F8F\x1F98\x1F99\x1F9A"
      U"\x1F9B\x1F9C\x1F9D\x1F9E\x1F9F\x1FA8\x1FA9\x1FAA\x1FAB\x1FAC\x1FAD\x1FAE\x1FAF\x1FB8\x1FB9"
      U"\x1FD8\x1FD9\x1FE8\x1FE9";

  if (wc >= from[0] && wc <= from[ARRAY_SIZE(from) - 2]) {
    /* Binary search since these are sorted. */
    size_t min = 0;
    size_t max = ARRAY_SIZE(from) - 2;
    while (max >= min) {
      const size_t mid = (min + max) / 2;
      if (wc > from[mid]) {
        min = mid + 1;
      }
      else if (wc < from[mid]) {
        max = mid - 1;
      }
      else {
        return to[mid];
      }
    }
  }

  return wc;
}

char32_t BLI_str_utf32_char_to_lower(const char32_t wc)
{
  if (wc < U'\xD8') { /* Latin. */
    if ((wc <= U'Z' && wc >= U'A') || (wc <= U'\xD6' && wc >= U'\xC0')) {
      return wc + 32;
    }
    return wc;
  }
  if ((wc <= U'\x136' && wc >= U'\x100') || (wc <= U'\x1E94' && wc >= U'\x1E00')) {
    /* Latin Extended. */
    return (wc % 2 == 0) ? wc + 1 : wc;
  }
  if ((wc <= U'\x556' && wc >= U'\x531') || (wc <= U'\x10C5' && wc >= U'\x10A0')) {
    /* Armenian and Georgian. */
    return wc + 48;
  }
  if (wc <= U'\x24CF' && wc >= U'\x24B6') { /* Enclosed Numerals. */
    return wc + 26;
  }
  if (wc <= U'\xFF3A' && wc >= U'\xFF21') { /* Full-width Forms. */
    return wc + 32;
  }

  /* There are only three remaining ranges that contain capitalization. */
  if (!(wc <= U'\x0216' && wc >= U'\x00D8') && !(wc <= U'\x04F8' && wc >= U'\x0386') &&
      !(wc <= U'\x1FE9' && wc >= U'\x1E00'))
  {
    return wc;
  }

  static const char32_t from[] =
      U"\x00D8\x00D9\x00DA\x00DB\x00DC\x00DD\x00DE\x0139\x013B\x013D\x013F\x0141\x0143\x0145\x0147"
      U"\x014A\x014C\x014E\x0150\x0152\x0154\x0156\x0158\x015A\x015C\x015E\x0160\x0162\x0164\x0166"
      U"\x0168\x016A\x016C\x016E\x0170\x0172\x0174\x0176\x0178\x0179\x017B\x017D\x0181\x0182\x0184"
      U"\x0186\x0187\x018A\x018B\x018E\x018F\x0190\x0191\x0193\x0194\x0196\x0197\x0198\x019C\x019D"
      U"\x019F\x01A0\x01A2\x01A4\x01A7\x01A9\x01AC\x01AE\x01AF\x01B1\x01B2\x01B3\x01B5\x01B7\x01B8"
      U"\x01BC\x01C4\x01C5\x01C7\x01C8\x01CA\x01CB\x01CD\x01CF\x01D1\x01D3\x01D5\x01D7\x01D9\x01DB"
      U"\x01DE\x01E0\x01E2\x01E4\x01E6\x01E8\x01EA\x01EC\x01EE\x01F1\x01F4\x01FA\x01FC\x01FE\x0200"
      U"\x0202\x0204\x0206\x0208\x020A\x020C\x020E\x0210\x0212\x0214\x0216\x0386\x0388\x0389\x038A"
      U"\x038C\x038E\x038F\x0391\x0392\x0393\x0394\x0395\x0396\x0397\x0398\x0399\x039A\x039B\x039C"
      U"\x039D\x039E\x039F\x03A0\x03A1\x03A3\x03A4\x03A5\x03A6\x03A7\x03A8\x03A9\x03AA\x03AB\x03E2"
      U"\x03E4\x03E6\x03E8\x03EA\x03EC\x03EE\x0401\x0402\x0403\x0404\x0405\x0406\x0407\x0408\x0409"
      U"\x040A\x040B\x040C\x040E\x040F\x0410\x0411\x0412\x0413\x0414\x0415\x0416\x0417\x0418\x0419"
      U"\x041A\x041B\x041C\x041D\x041E\x041F\x0420\x0421\x0422\x0423\x0424\x0425\x0426\x0427\x0428"
      U"\x0429\x042A\x042B\x042C\x042D\x042E\x042F\x0460\x0462\x0464\x0466\x0468\x046A\x046C\x046E"
      U"\x0470\x0472\x0474\x0476\x0478\x047A\x047C\x047E\x0480\x0490\x0492\x0494\x0496\x0498\x049A"
      U"\x049C\x049E\x04A0\x04A2\x04A4\x04A6\x04A8\x04AA\x04AC\x04AE\x04B0\x04B2\x04B4\x04B6\x04B8"
      U"\x04BA\x04BC\x04BE\x04C1\x04C3\x04C7\x04CB\x04D0\x04D2\x04D4\x04D6\x04D8\x04DA\x04DC\x04DE"
      U"\x04E0\x04E2\x04E4\x04E6\x04E8\x04EA\x04EE\x04F0\x04F2\x04F4\x04F8\x1EA0\x1EA2\x1EA4\x1EA6"
      U"\x1EA8\x1EAA\x1EAC\x1EAE\x1EB0\x1EB2\x1EB4\x1EB6\x1EB8\x1EBA\x1EBC\x1EBE\x1EC0\x1EC2\x1EC4"
      U"\x1EC6\x1EC8\x1ECA\x1ECC\x1ECE\x1ED0\x1ED2\x1ED4\x1ED6\x1ED8\x1EDA\x1EDC\x1EDE\x1EE0\x1EE2"
      U"\x1EE4\x1EE6\x1EE8\x1EEA\x1EEC\x1EEE\x1EF0\x1EF2\x1EF4\x1EF6\x1EF8\x1F08\x1F09\x1F0A\x1F0B"
      U"\x1F0C\x1F0D\x1F0E\x1F0F\x1F18\x1F19\x1F1A\x1F1B\x1F1C\x1F1D\x1F28\x1F29\x1F2A\x1F2B\x1F2C"
      U"\x1F2D\x1F2E\x1F2F\x1F38\x1F39\x1F3A\x1F3B\x1F3C\x1F3D\x1F3E\x1F3F\x1F48\x1F49\x1F4A\x1F4B"
      U"\x1F4C\x1F4D\x1F59\x1F5B\x1F5D\x1F5F\x1F68\x1F69\x1F6A\x1F6B\x1F6C\x1F6D\x1F6E\x1F6F\x1F88"
      U"\x1F89\x1F8A\x1F8B\x1F8C\x1F8D\x1F8E\x1F8F\x1F98\x1F99\x1F9A\x1F9B\x1F9C\x1F9D\x1F9E\x1F9F"
      U"\x1FA8\x1FA9\x1FAA\x1FAB\x1FAC\x1FAD\x1FAE\x1FAF\x1FB8\x1FB9\x1FD8\x1FD9\x1FE8\x1FE9";
  static const char32_t to[] =
      U"\x00F8\x00F9\x00FA\x00FB\x00FC\x00FD\x00FE\x013A\x013C\x013E\x0140\x0142\x0144\x0146\x0148"
      U"\x014B\x014D\x014F\x0151\x0153\x0155\x0157\x0159\x015B\x015D\x015F\x0161\x0163\x0165\x0167"
      U"\x0169\x016B\x016D\x016F\x0171\x0173\x0175\x0177\x00FF\x017A\x017C\x017E\x0253\x0183\x0185"
      U"\x0254\x0188\x0257\x018C\x0258\x0259\x025B\x0192\x0260\x0263\x0269\x0268\x0199\x026f\x0272"
      U"\x0275\x01A1\x01A3\x01A5\x01A8\x0283\x01AD\x0288\x01B0\x028A\x028B\x01B4\x01B6\x0292\x01B9"
      U"\x01BD\x01C6\x01C6\x01C9\x01C9\x01CC\x01CC\x01CE\x01D0\x01D2\x01D4\x01D6\x01D8\x01DA\x01DC"
      U"\x01DF\x01E1\x01E3\x01E5\x01E7\x01E9\x01EB\x01ED\x01EF\x01F3\x01F5\x01FB\x01FD\x01FF\x0201"
      U"\x0203\x0205\x0207\x0209\x020B\x020D\x020F\x0211\x0213\x0215\x0217\x03AC\x03AD\x03AE\x03AF"
      U"\x03CC\x03CD\x03CE\x03B1\x03B2\x03B3\x03B4\x03B5\x03B6\x03B7\x03B8\x03B9\x03BA\x03BB\x03BC"
      U"\x03BD\x03BE\x03BF\x03C0\x03C1\x03C3\x03C4\x03C5\x03C6\x03C7\x03C8\x03C9\x03CA\x03CB\x03E3"
      U"\x03E5\x03E7\x03E9\x03EB\x03ED\x03EF\x0451\x0452\x0453\x0454\x0455\x0456\x0457\x0458\x0459"
      U"\x045A\x045B\x045C\x045E\x045F\x0430\x0431\x0432\x0433\x0434\x0435\x0436\x0437\x0438\x0439"
      U"\x043A\x043B\x043C\x043D\x043E\x043F\x0440\x0441\x0442\x0443\x0444\x0445\x0446\x0447\x0448"
      U"\x0449\x044A\x044B\x044C\x044D\x044E\x044F\x0461\x0463\x0465\x0467\x0469\x046B\x046D\x046F"
      U"\x0471\x0473\x0475\x0477\x0479\x047B\x047D\x047F\x0481\x0491\x0493\x0495\x0497\x0499\x049B"
      U"\x049D\x049F\x04A1\x04A3\x04A5\x04A7\x04A9\x04AB\x04AD\x04AF\x04B1\x04B3\x04B5\x04B7\x04B9"
      U"\x04BB\x04BD\x04BF\x04C2\x04C4\x04C8\x04CC\x04D1\x04D3\x04D5\x04D7\x04D9\x04DB\x04DD\x04DF"
      U"\x04E1\x04E3\x04E5\x04E7\x04E9\x04EB\x04EF\x04F1\x04F3\x04F5\x04F9\x1EA1\x1EA3\x1EA5\x1EA7"
      U"\x1EA9\x1EAB\x1EAD\x1EAF\x1EB1\x1EB3\x1EB5\x1EB7\x1EB9\x1EBB\x1EBD\x1EBF\x1EC1\x1EC3\x1EC5"
      U"\x1EC7\x1EC9\x1ECB\x1ECD\x1ECF\x1ED1\x1ED3\x1ED5\x1ED7\x1ED9\x1EDB\x1EDD\x1EDF\x1EE1\x1EE3"
      U"\x1EE5\x1EE7\x1EE9\x1EEB\x1EED\x1EEF\x1EF1\x1EF3\x1EF5\x1EF7\x1EF9\x1F00\x1F01\x1F02\x1F03"
      U"\x1F04\x1F05\x1F06\x1F07\x1F10\x1F11\x1F12\x1F13\x1F14\x1F15\x1F20\x1F21\x1F22\x1F23\x1F24"
      U"\x1F25\x1F26\x1F27\x1F30\x1F31\x1F32\x1F33\x1F34\x1F35\x1F36\x1F37\x1F40\x1F41\x1F42\x1F43"
      U"\x1F44\x1F45\x1F51\x1F53\x1F55\x1F57\x1F60\x1F61\x1F62\x1F63\x1F64\x1F65\x1F66\x1F67\x1F80"
      U"\x1F81\x1F82\x1F83\x1F84\x1F85\x1F86\x1F87\x1F90\x1F91\x1F92\x1F93\x1F94\x1F95\x1F96\x1F97"
      U"\x1FA0\x1FA1\x1FA2\x1FA3\x1FA4\x1FA5\x1FA6\x1FA7\x1FB0\x1FB1\x1FD0\x1FD1\x1FE0\x1FE1";

  if (wc >= from[0] && wc <= from[ARRAY_SIZE(from) - 2]) {
    /* Binary search since these are sorted. */
    size_t min = 0;
    size_t max = ARRAY_SIZE(from) - 2;
    while (max >= min) {
      const size_t mid = (min + max) / 2;
      if (wc > from[mid]) {
        min = mid + 1;
      }
      else if (wc < from[mid]) {
        max = mid - 1;
      }
      else {
        return to[mid];
      }
    }
  }

  return wc;
}

/* -------------------------------------------------------------------- */
/** \name UTF32 Text Boundary Analysis
 *
 * Helper functions to help locating linguistic boundaries, like word,
 * sentence, and paragraph boundaries.
 * \{ */

bool BLI_str_utf32_char_is_breaking_space(char32_t codepoint)
{
  /* Invisible (and so can be removed at end of wrapped line) spacing characters
   * according to the Unicode Line Breaking Algorithm (Standard Annex #14). Note
   * to always ignore U+200B (zero-width space) and U+2060 (word joiner). */
  return ELEM(codepoint,
              ' ',     /* Space. */
              0x1680,  /* Ogham space mark. */
              0x2000,  /* En quad. */
              0x2001,  /* Em quad. */
              0x2002,  /* En space. */
              0x2003,  /* Em space. */
              0x2004,  /* Three-per-em space. */
              0x2005,  /* Four-per-em space. */
              0x2006,  /* Six-per-em space. */
              0x2008,  /* Punctuation space. */
              0x2009,  /* Thin space. */
              0x200A,  /* Hair space. */
              0x205F,  /* Medium mathematical space. */
              0x3000); /* Ideographic space. */
}

bool BLI_str_utf32_char_is_optional_break_after(char32_t codepoint, char32_t codepoint_prev)
{
  /* Subset of the characters that are line breaking opportunities
   * according to the Unicode Line Breaking Algorithm (Standard Annex #14).
   * Can be expanded but please no rules that differ by language. */

  /* Punctuation. Backslash can be used as path separator */
  if (ELEM(codepoint, '\\', '_')) {
    return true;
  }

  /* Do not break on solidus if previous is a number. */
  if (codepoint == '/' && !(codepoint_prev >= '0' && codepoint_prev <= '9')) {
    return true;
  }

  /* Do not break on dash, hyphen, em dash if previous is space */
  if (ELEM(codepoint, '-', 0x2010, 0x2014) &&
      !BLI_str_utf32_char_is_breaking_space(codepoint_prev))
  {
    return true;
  }

  if ((codepoint >= 0x2E80 && codepoint <= 0x2FFF) || /* CJK, Kangxi Radicals. */
      (codepoint >= 0x3040 && codepoint <= 0x309F) || /* Hiragana (except small characters). */
      (codepoint >= 0x30A2 && codepoint <= 0x30FA) || /* Katakana (except small characters). */
      (codepoint >= 0x3400 && codepoint <= 0x4DBF) || /* CJK Unified Ideographs Extension A. */
      (codepoint >= 0x4E00 && codepoint <= 0x9FFF) || /* CJK Unified Ideographs. */
      (codepoint >= 0x3040 && codepoint <= 0x309F) || /* CJK Unified Ideographs. */
      (codepoint >= 0x3130 && codepoint <= 0x318F))   /* Hangul Compatibility Jamo. */
  {
    return true;
  }

  if (ELEM(codepoint, 0x0F0D, 0x0F0B)) {
    return true; /* Tibetan shad mark and intersyllabic tsheg. */
  }

  return false;
}

bool BLI_str_utf32_char_is_optional_break_before(char32_t codepoint, char32_t codepoint_prev)
{
  /* Do not break on any of these if a space follows. */
  if (BLI_str_utf32_char_is_breaking_space(codepoint)) {
    return false;
  }

  /* Infix Numeric Separators. Allow break on these if not numbers afterward. */
  if (ELEM(codepoint_prev,
           ',',    /* Comma. */
           ':',    /* Colon. */
           ';',    /* Semicolon. */
           0x037E, /* Greek question mark. */
           0x0589, /* Armenian full stop. */
           0x060C, /* Arabic comma. */
           0x060D, /* Arabic date separator. */
           0x07F8, /* N'Ko comma. */
           0x2044) /* Fraction slash. */
      && !(codepoint >= '0' && codepoint <= '9'))
  {
    return true;
  }

  /* Break on full stop only if not followed by another, or by a number. */
  if (codepoint_prev == '.' && codepoint != '.' && !(codepoint >= '0' && codepoint <= '9')) {
    return true;
  }

  /* Close punctuation. */
  if (ELEM(codepoint_prev,
           0x3001,  /* Ideographic comma. */
           0x3002,  /* Ideographic full stop. */
           0xFE10,  /* Presentation form for vertical ideographic comma. */
           0xFE11,  /* Presentation form for vertical ideographic full stop. */
           0xFE12,  /* Presentation form for vertical ideographic colon. */
           0xFE50,  /* Small comma. */
           0xFE52,  /* Small full stop. */
           0xFF0C,  /* Full-width comma. */
           0xFF0E,  /* Full-width full stop. */
           0XFF61,  /* Half-width ideographic full stop. */
           0Xff64)) /* Half-width ideographic comma. */
  {
    return true;
  }

  /* Exclamation/Interrogation. */
  if (ELEM(codepoint_prev,
           '!',     /* Exclamation mark. */
           '?',     /* Question mark. */
           0x05C6,  /* Hebrew punctuation `maqaf`. */
           0x061B,  /* Arabic semicolon. */
           0x061E,  /* Arabic triple dot. */
           0x061F,  /* Arabic question mark. */
           0x06D4,  /* Arabic full stop. */
           0x07F9,  /* N'Ko question mark. */
           0x0F0D,  /* Tibetan shad mark. */
           0xFF01,  /* Full-width exclamation mark. */
           0xff1f)) /* full-width question mark. */
  {
    return true;
  }

  return false;
}

bool BLI_str_utf32_char_is_terminal_punctuation(char32_t codepoint)
{
  /* Characters marking the end of sentences according to Unicode Text Segmentation
   * (Standard Annex #29), Sentence Break Property (ATerm, STerm).
   * Only the characters available in Blender are matched. */

  return (ELEM(codepoint,
               0x002E,    /* Full stop. */
               0x2024,    /* One dot leader. */
               0xFE52,    /* Small full stop. */
               0xFF0E,    /* Fullwidth full stop. */
               0x0021,    /* Exclamation mark. */
               0x003F,    /* Question mark. */
               0x0589,    /* Armenian full stop. */
               0x061F,    /* Arabic question mark. */
               0x06D4,    /* Arabic full stop. */
               0x0964,    /* Devanagari danda. */
               0x0965,    /* Devanagari double danda. */
               0x104A,    /* Myanmar sign little section. */
               0x104B,    /* Myanmar sign section. */
               0x1362,    /* Ethiopic full stop. */
               0x1367,    /* Ethiopic question mark. */
               0x1368) || /* Ethiopic paragraph separator. */
          ELEM(codepoint,
               0x17D4,    /* Khmer sign khan. */
               0x17D5,    /* Khmer sign bariyoosan. */
               0x203C,    /* Double exclamation mark. */
               0x203D,    /* Interrobang. */
               0x2047,    /* Double question mark. */
               0x2048,    /* Question exclamation mark. */
               0x2049,    /* Exclamation question mark. */
               0x3002,    /* Ideographic full stop. */
               0xA9C8,    /* Javanese pada lingsa. */
               0xA9C9,    /* Javanese pada lungsi. */
               0xFE12,    /* Presentation form for vertical ideographic full stop. */
               0xFE15,    /* Presentation form for vertical exclamation mark. */
               0xFE16,    /* Presentation form for vertical question mark. */
               0xFE56,    /* Small question mark. */
               0xFE57,    /* Small exclamation mark. */
               0xFF01) || /* Fullwidth exclamation mark. */
          ELEM(codepoint,
               0xFF1F,   /* Fullwidth question mark. */
               0xFF61)); /* Halfwidth ideographic full stop. */
}

/** \} */ /* -------------------------------------------------------------------- */

int BLI_str_utf8_size_or_error(const char *p)
{
  return utf8_char_compute_skip_or_error(*p);
}

int BLI_str_utf8_size_safe(const char *p)
{
  return utf8_char_compute_skip(*p);
}

uint BLI_str_utf8_as_unicode_or_error(const char *p)
{
  /* Originally `g_utf8_get_char` in GLIB. */

  const uchar c = uchar(*p);

  char mask = 0;
  const int len = utf8_char_compute_skip_or_error_with_mask(c, &mask);
  if (UNLIKELY(len == -1)) {
    return BLI_UTF8_ERR;
  }
  return utf8_char_decode(p, mask, len, BLI_UTF8_ERR);
}

uint BLI_str_utf8_as_unicode_safe(const char *p)
{
  const uint result = BLI_str_utf8_as_unicode_or_error(p);
  if (UNLIKELY(result == BLI_UTF8_ERR)) {
    return *p;
  }
  return result;
}

uint BLI_str_utf8_as_unicode_step_or_error(const char *__restrict p,
                                           const size_t p_len,
                                           size_t *__restrict index)
{
  const uchar c = uchar(*(p += *index));

  BLI_assert(*index < p_len);
  BLI_assert(c != '\0');

  char mask = 0;
  const int len = utf8_char_compute_skip_or_error_with_mask(c, &mask);
  if (UNLIKELY(len == -1) || (*index + size_t(len) > p_len)) {
    return BLI_UTF8_ERR;
  }

  const uint result = utf8_char_decode(p, mask, len, BLI_UTF8_ERR);
  if (UNLIKELY(result == BLI_UTF8_ERR)) {
    return BLI_UTF8_ERR;
  }
  *index += size_t(len);
  BLI_assert(*index <= p_len);
  return result;
}

uint BLI_str_utf8_as_unicode_step_safe(const char *__restrict p,
                                       const size_t p_len,
                                       size_t *__restrict index)
{
  uint result = BLI_str_utf8_as_unicode_step_or_error(p, p_len, index);
  if (UNLIKELY(result == BLI_UTF8_ERR)) {
    result = uint(p[*index]);
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

size_t BLI_str_utf8_from_unicode(uint c, char *dst, const size_t dst_maxncpy)

{
  BLI_string_debug_size(dst, dst_maxncpy);

  /* If this gets modified, also update the copy in g_string_insert_unichar() */
  uint len = 0;
  uint first;

  UTF8_VARS_FROM_CHAR32(c, first, len);

  if (UNLIKELY(dst_maxncpy < len)) {
    /* Null terminate instead of writing a partial byte. */
    memset(dst, 0x0, dst_maxncpy);
    return dst_maxncpy;
  }

  for (uint i = len - 1; i > 0; i--) {
    dst[i] = char((c & 0x3f) | 0x80);
    c >>= 6;
  }
  dst[0] = char(c | first);

  return len;
}

size_t BLI_str_utf8_as_utf32(char32_t *__restrict dst_w,
                             const char *__restrict src_c,
                             const size_t dst_w_maxncpy)
{
  BLI_assert(dst_w_maxncpy != 0);
  BLI_string_debug_size(dst_w, dst_w_maxncpy);

  const size_t maxlen = dst_w_maxncpy - 1;
  size_t len = 0;

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
      index = size_t(src_c_next - src_c);
    }
    dst_w++;
    len++;
  }

  *dst_w = 0;

  return len;
}

size_t BLI_str_utf32_as_utf8(char *__restrict dst,
                             const char32_t *__restrict src,
                             const size_t dst_maxncpy)
{
  BLI_assert(dst_maxncpy != 0);
  BLI_string_debug_size(dst, dst_maxncpy);

  size_t len = 0;
  while (*src && len < dst_maxncpy) {
    len += BLI_str_utf8_from_unicode(uint(*src++), dst + len, dst_maxncpy - len);
  }
  dst[len] = '\0';
  /* Return the correct length when part of the final byte did not fit into the string. */
  while ((len > 0) && UNLIKELY(dst[len - 1] == '\0')) {
    len--;
  }
  return len;
}

size_t BLI_str_utf32_as_utf8_len_ex(const char32_t *src, const size_t src_maxlen)
{
  size_t len = 0;
  const char32_t *src_end = src + src_maxlen;

  while ((src < src_end) && *src) {
    len += BLI_str_utf8_from_unicode_len(uint(*src++));
  }

  return len;
}

size_t BLI_str_utf32_as_utf8_len(const char32_t *src)
{
  size_t len = 0;

  while (*src) {
    len += BLI_str_utf8_from_unicode_len(uint(*src++));
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
  return BLI_str_partition_ex_utf8(str, nullptr, delim, r_sep, r_suf, false);
}

size_t BLI_str_rpartition_utf8(const char *str,
                               const uint delim[],
                               const char **r_sep,
                               const char **r_suf)
{
  return BLI_str_partition_ex_utf8(str, nullptr, delim, r_sep, r_suf, true);
}

size_t BLI_str_partition_ex_utf8(const char *str,
                                 const char *end,
                                 const uint delim[],
                                 const char **r_sep,
                                 const char **r_suf,
                                 const bool from_right)
{
  const size_t str_len = end ? size_t(end - str) : strlen(str);
  if (end == nullptr) {
    end = str + str_len;
  }

  /* Note that here, we assume end points to a valid UTF8 char! */
  BLI_assert((end >= str) && (BLI_str_utf8_as_unicode_or_error(end) != BLI_UTF8_ERR));

  char *suf = (char *)(str + str_len);
  size_t index = 0;
  for (char *sep = (char *)(from_right ? BLI_str_find_prev_char_utf8(end, str) : str);
       from_right ? (sep > str) : ((sep < end) && (*sep != '\0'));
       sep = (char *)(from_right ? (str != sep ? BLI_str_find_prev_char_utf8(sep, str) : nullptr) :
                                   str + index))
  {
    size_t index_ofs = 0;
    const uint c = BLI_str_utf8_as_unicode_step_or_error(sep, size_t(end - sep), &index_ofs);
    if (UNLIKELY(c == BLI_UTF8_ERR)) {
      break;
    }
    index += index_ofs;

    for (const uint *d = delim; *d != '\0'; d++) {
      if (*d == c) {
        /* `suf` is already correct in case from_right is true. */
        *r_sep = sep;
        *r_suf = from_right ? suf : (char *)(str + index);
        return size_t(sep - str);
      }
    }

    suf = sep; /* Useful in 'from_right' case! */
  }

  *r_suf = *r_sep = nullptr;
  return str_len;
}

/**
 * It's always assumed trimming is needed, otherwise call #BLI_str_utf8_truncate_at_size.
 */
static size_t str_utf8_truncate_at_size_unchecked(char *str, const size_t str_size)
{
  BLI_assert(str_size > 0);
  BLI_assert(!std::memchr(str, '\0', str_size - 1));
  size_t str_len_trim;
  BLI_strnlen_utf8_ex(str, str_size - 1, &str_len_trim);
  str[str_len_trim] = '\0';
  return str_len_trim;
}

bool BLI_str_utf8_truncate_at_size(char *str, const size_t str_size)
{
  BLI_assert(str_size > 0);
  if (std::memchr(str, '\0', str_size)) {
    return false;
  }

  str_utf8_truncate_at_size_unchecked(str, str_size);
  return true;
}

/* -------------------------------------------------------------------- */
/** \name Offset Conversion in Strings
 *
 * \note Regarding the assertion: `BLI_assert(offset <= offset_target)`
 * The `offset_target` is likely in the middle of a UTF8 byte-sequence.
 * Most likely the offset passed in is incorrect, although it may be impractical to
 * avoid this happening in the case of invalid UTF8 byte sequences.
 * If the assert is impractical to avoid, it could be demoted to a warning.
 * \{ */

int BLI_str_utf8_offset_to_index(const char *str, const size_t str_len, const int offset_target)
{
  BLI_assert(offset_target >= 0);
  const size_t offset_target_as_size = size_t(offset_target);
  size_t offset = 0;
  int index = 0;
  /* Note that `offset != offset_target_as_size` works for valid UTF8 strings. */
  while ((offset < str_len) && (offset < offset_target_as_size)) {
    /* Use instead of #BLI_str_utf8_size_safe to match behavior when limiting the string length. */
    const uint code = BLI_str_utf8_as_unicode_step_safe(str, str_len, &offset);
    UNUSED_VARS(code);
    index++;
    BLI_assert(offset <= offset_target_as_size); /* See DOXY section comment. */
  }
  return index;
}

int BLI_str_utf8_offset_from_index(const char *str, const size_t str_len, const int index_target)
{
  BLI_assert(index_target >= 0);
  size_t offset = 0;
  int index = 0;
  while ((offset < str_len) && (index < index_target)) {
    /* Use instead of #BLI_str_utf8_size_safe to match behavior when limiting the string length. */
    const uint code = BLI_str_utf8_as_unicode_step_safe(str, str_len, &offset);
    UNUSED_VARS(code);
    index++;
  }
  return int(offset);
}

int BLI_str_utf8_offset_to_column(const char *str, const size_t str_len, const int offset_target)
{
  BLI_assert(offset_target >= 0);
  const size_t offset_target_clamp = std::min(size_t(offset_target), str_len);
  size_t offset = 0;
  int column = 0;
  while (offset < offset_target_clamp) {
    const uint code = BLI_str_utf8_as_unicode_step_safe(str, str_len, &offset);
    column += BLI_wcwidth_safe(code);
    BLI_assert(offset <= size_t(offset_target)); /* See DOXY section comment. */
  }
  return column;
}

int BLI_str_utf8_offset_from_column(const char *str, const size_t str_len, const int column_target)
{
  size_t offset = 0, offset_next = 0;
  int column = 0;
  while ((offset < str_len) && (column < column_target)) {
    const uint code = BLI_str_utf8_as_unicode_step_safe(str, str_len, &offset_next);
    column += BLI_wcwidth_safe(code);
    if (column > column_target) {
      break;
    }
    offset = offset_next;
  }
  return int(offset);
}

int BLI_str_utf8_offset_to_column_with_tabs(const char *str,
                                            const size_t str_len,
                                            const int offset_target,
                                            const int tab_width)
{
  BLI_assert(offset_target >= 0);
  const size_t offset_target_clamp = std::min(size_t(offset_target), str_len);
  size_t offset = 0;
  int column = 0;
  while (offset < offset_target_clamp) {
    const uint code = BLI_str_utf8_as_unicode_step_safe(str, str_len, &offset);
    /* The following line is the only change compared with #BLI_str_utf8_offset_to_column. */
    column += (code == '\t') ? (tab_width - (column % tab_width)) : BLI_wcwidth_safe(code);
    BLI_assert(offset <= size_t(offset_target)); /* See DOXY section comment. */
  }
  return column;
}

int BLI_str_utf8_offset_from_column_with_tabs(const char *str,
                                              const size_t str_len,
                                              const int column_target,
                                              const int tab_width)
{
  size_t offset = 0, offset_next = 0;
  int column = 0;
  while ((offset < str_len) && (column < column_target)) {
    const uint code = BLI_str_utf8_as_unicode_step_safe(str, str_len, &offset_next);
    /* The following line is the only change compared with #BLI_str_utf8_offset_from_column. */
    column += (code == '\t') ? (tab_width - (column % tab_width)) : BLI_wcwidth_safe(code);
    if (column > column_target) {
      break;
    }
    offset = offset_next;
  }
  return int(offset);
}

int BLI_str_utf8_column_count(const char *str, size_t str_len)
{
  return BLI_str_utf8_offset_to_column(str, str_len, int(str_len));
}

/** \} */
