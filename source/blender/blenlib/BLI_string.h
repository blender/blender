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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bli
 */

#include <inttypes.h>
#include <stdarg.h>

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

char *BLI_strdupn(const char *str, const size_t len) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

char *BLI_strdup(const char *str) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL() ATTR_MALLOC;

char *BLI_strdupcat(const char *__restrict str1,
                    const char *__restrict str2) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL() ATTR_MALLOC;

char *BLI_strncpy(char *__restrict dst, const char *__restrict src, const size_t maxncpy)
    ATTR_NONNULL();

char *BLI_strncpy_ensure_pad(char *__restrict dst,
                             const char *__restrict src,
                             const char pad,
                             size_t maxncpy) ATTR_NONNULL();

size_t BLI_strncpy_rlen(char *__restrict dst,
                        const char *__restrict src,
                        const size_t maxncpy) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

size_t BLI_strcpy_rlen(char *__restrict dst, const char *__restrict src) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

bool BLI_str_quoted_substr_range(const char *__restrict str,
                                 const char *__restrict prefix,
                                 int *__restrict r_start,
                                 int *__restrict r_end) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3, 4);
#if 0 /* UNUSED */
char *BLI_str_quoted_substrN(const char *__restrict str,
                             const char *__restrict prefix) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL() ATTR_MALLOC;
#endif
bool BLI_str_quoted_substr(const char *__restrict str,
                           const char *__restrict prefix,
                           char *result,
                           size_t result_maxlen);
char *BLI_str_replaceN(const char *__restrict str,
                       const char *__restrict substr_old,
                       const char *__restrict substr_new) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL() ATTR_MALLOC;

void BLI_str_replace_char(char *string, char src, char dst) ATTR_NONNULL();

bool BLI_str_replace_table_exact(char *string,
                                 const size_t string_len,
                                 const char *replace_table[][2],
                                 int replace_table_len);

size_t BLI_snprintf(char *__restrict dst, size_t maxncpy, const char *__restrict format, ...)
    ATTR_NONNULL(1, 3) ATTR_PRINTF_FORMAT(3, 4);
size_t BLI_snprintf_rlen(char *__restrict dst, size_t maxncpy, const char *__restrict format, ...)
    ATTR_NONNULL(1, 3) ATTR_PRINTF_FORMAT(3, 4);

size_t BLI_vsnprintf(char *__restrict buffer,
                     size_t maxncpy,
                     const char *__restrict format,
                     va_list arg) ATTR_PRINTF_FORMAT(3, 0);
size_t BLI_vsnprintf_rlen(char *__restrict buffer,
                          size_t maxncpy,
                          const char *__restrict format,
                          va_list arg) ATTR_PRINTF_FORMAT(3, 0);

char *BLI_sprintfN(const char *__restrict format, ...) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1) ATTR_MALLOC ATTR_PRINTF_FORMAT(1, 2);

size_t BLI_str_escape(char *__restrict dst, const char *__restrict src, const size_t dst_maxncpy)
    ATTR_NONNULL();
size_t BLI_str_unescape_ex(char *__restrict dst,
                           const char *__restrict src,
                           const size_t src_maxncpy,
                           /* Additional arguments. */
                           const size_t dst_maxncpy,
                           bool *r_is_complete) ATTR_NONNULL();
size_t BLI_str_unescape(char *__restrict dst, const char *__restrict src, const size_t src_maxncpy)
    ATTR_NONNULL();

const char *BLI_str_escape_find_quote(const char *str) ATTR_NONNULL();

size_t BLI_str_format_int_grouped(char dst[16], int num) ATTR_NONNULL();
size_t BLI_str_format_uint64_grouped(char dst[16], uint64_t num) ATTR_NONNULL();
void BLI_str_format_byte_unit(char dst[15], long long int bytes, const bool base_10)
    ATTR_NONNULL();
void BLI_str_format_attribute_domain_size(char dst[7], int number_to_format) ATTR_NONNULL();
int BLI_strcaseeq(const char *a, const char *b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
char *BLI_strcasestr(const char *s, const char *find) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
char *BLI_strncasestr(const char *s, const char *find, size_t len) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
int BLI_strcasecmp(const char *s1, const char *s2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int BLI_strncasecmp(const char *s1, const char *s2, size_t len) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
int BLI_strcasecmp_natural(const char *s1, const char *s2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int BLI_strcmp_ignore_pad(const char *str1,
                          const char *str2,
                          const char pad) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

size_t BLI_strnlen(const char *str, const size_t maxlen) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

void BLI_str_tolower_ascii(char *str, const size_t len) ATTR_NONNULL();
void BLI_str_toupper_ascii(char *str, const size_t len) ATTR_NONNULL();
void BLI_str_rstrip(char *str) ATTR_NONNULL();
int BLI_str_rstrip_float_zero(char *str, const char pad) ATTR_NONNULL();

int BLI_str_index_in_array_n(const char *__restrict str,
                             const char **__restrict str_array,
                             const int str_array_len) ATTR_NONNULL();
int BLI_str_index_in_array(const char *__restrict str, const char **__restrict str_array)
    ATTR_NONNULL();

bool BLI_str_startswith(const char *__restrict str, const char *__restrict start) ATTR_NONNULL();
bool BLI_str_endswith(const char *__restrict str, const char *__restrict end) ATTR_NONNULL();
bool BLI_strn_endswith(const char *__restrict str, const char *__restrict end, size_t length)
    ATTR_NONNULL();

size_t BLI_str_partition(const char *str, const char delim[], const char **sep, const char **suf)
    ATTR_NONNULL();
size_t BLI_str_rpartition(const char *str, const char delim[], const char **sep, const char **suf)
    ATTR_NONNULL();
size_t BLI_str_partition_ex(const char *str,
                            const char *end,
                            const char delim[],
                            const char **sep,
                            const char **suf,
                            const bool from_right) ATTR_NONNULL(1, 3, 4, 5);

int BLI_string_max_possible_word_count(const int str_len);
bool BLI_string_has_word_prefix(const char *haystack, const char *needle, size_t needle_len);
bool BLI_string_all_words_matched(const char *name,
                                  const char *str,
                                  int (*words)[2],
                                  const int words_len);

int BLI_string_find_split_words(const char *str,
                                const size_t len,
                                const char delim,
                                int r_words[][2],
                                int words_max) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* -------------------------------------------------------------------- */
/** \name String Copy/Format Macros
 * Avoid repeating destination with `sizeof(..)`.
 * \note `ARRAY_SIZE` allows pointers on some platforms.
 * \{ */
#define STRNCPY(dst, src) BLI_strncpy(dst, src, ARRAY_SIZE(dst))
#define STRNCPY_RLEN(dst, src) BLI_strncpy_rlen(dst, src, ARRAY_SIZE(dst))
#define SNPRINTF(dst, format, ...) BLI_snprintf(dst, ARRAY_SIZE(dst), format, __VA_ARGS__)
#define SNPRINTF_RLEN(dst, format, ...) \
  BLI_snprintf_rlen(dst, ARRAY_SIZE(dst), format, __VA_ARGS__)
#define STR_CONCAT(dst, len, suffix) \
  len += BLI_strncpy_rlen(dst + len, suffix, ARRAY_SIZE(dst) - len)
#define STR_CONCATF(dst, len, format, ...) \
  len += BLI_snprintf_rlen(dst + len, ARRAY_SIZE(dst) - len, format, __VA_ARGS__)
/** \} */

/* -------------------------------------------------------------------- */
/** \name Equal to Any Element (STR_ELEM) Macro
 *
 * Follows #ELEM macro convention.
 * \{ */

/* Manual line breaks for readability. */
/* clang-format off */
/* STR_ELEM#(v, ...): is the first arg equal any others? */
/* Internal helpers. */
#define _VA_STR_ELEM2(v, a) (strcmp(v, a) == 0)
#define _VA_STR_ELEM3(v, a, b) \
  (_VA_STR_ELEM2(v, a) || (_VA_STR_ELEM2(v, b)))
#define _VA_STR_ELEM4(v, a, b, c) \
  (_VA_STR_ELEM3(v, a, b) || (_VA_STR_ELEM2(v, c)))
#define _VA_STR_ELEM5(v, a, b, c, d) \
  (_VA_STR_ELEM4(v, a, b, c) || (_VA_STR_ELEM2(v, d)))
#define _VA_STR_ELEM6(v, a, b, c, d, e) \
  (_VA_STR_ELEM5(v, a, b, c, d) || (_VA_STR_ELEM2(v, e)))
#define _VA_STR_ELEM7(v, a, b, c, d, e, f) \
  (_VA_STR_ELEM6(v, a, b, c, d, e) || (_VA_STR_ELEM2(v, f)))
#define _VA_STR_ELEM8(v, a, b, c, d, e, f, g) \
  (_VA_STR_ELEM7(v, a, b, c, d, e, f) || (_VA_STR_ELEM2(v, g)))
#define _VA_STR_ELEM9(v, a, b, c, d, e, f, g, h) \
  (_VA_STR_ELEM8(v, a, b, c, d, e, f, g) || (_VA_STR_ELEM2(v, h)))
#define _VA_STR_ELEM10(v, a, b, c, d, e, f, g, h, i) \
  (_VA_STR_ELEM9(v, a, b, c, d, e, f, g, h) || (_VA_STR_ELEM2(v, i)))
#define _VA_STR_ELEM11(v, a, b, c, d, e, f, g, h, i, j) \
  (_VA_STR_ELEM10(v, a, b, c, d, e, f, g, h, i) || (_VA_STR_ELEM2(v, j)))
#define _VA_STR_ELEM12(v, a, b, c, d, e, f, g, h, i, j, k) \
  (_VA_STR_ELEM11(v, a, b, c, d, e, f, g, h, i, j) || (_VA_STR_ELEM2(v, k)))
#define _VA_STR_ELEM13(v, a, b, c, d, e, f, g, h, i, j, k, l) \
  (_VA_STR_ELEM12(v, a, b, c, d, e, f, g, h, i, j, k) || (_VA_STR_ELEM2(v, l)))
#define _VA_STR_ELEM14(v, a, b, c, d, e, f, g, h, i, j, k, l, m) \
  (_VA_STR_ELEM13(v, a, b, c, d, e, f, g, h, i, j, k, l) || (_VA_STR_ELEM2(v, m)))
#define _VA_STR_ELEM15(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n) \
  (_VA_STR_ELEM14(v, a, b, c, d, e, f, g, h, i, j, k, l, m) || (_VA_STR_ELEM2(v, n)))
#define _VA_STR_ELEM16(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) \
  (_VA_STR_ELEM15(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n) || (_VA_STR_ELEM2(v, o)))
#define _VA_STR_ELEM17(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
  (_VA_STR_ELEM16(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) || (_VA_STR_ELEM2(v, p)))
/* clang-format on */

/* reusable STR_ELEM macro */
#define STR_ELEM(...) VA_NARGS_CALL_OVERLOAD(_VA_STR_ELEM, __VA_ARGS__)

/** \} */

#ifdef __cplusplus
}
#endif
