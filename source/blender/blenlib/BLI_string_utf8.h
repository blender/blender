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
 */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

char *BLI_strncpy_utf8(char *__restrict dst, const char *__restrict src, size_t maxncpy)
    ATTR_NONNULL(1, 2);
size_t BLI_strncpy_utf8_rlen(char *__restrict dst,
                             const char *__restrict src,
                             size_t maxncpy) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);
ptrdiff_t BLI_str_utf8_invalid_byte(const char *str, size_t length) ATTR_NONNULL(1);
int BLI_str_utf8_invalid_strip(char *str, size_t length) ATTR_NONNULL(1);

/* warning, can return -1 on bad chars */
int BLI_str_utf8_size(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLI_str_utf8_size_safe(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/* copied from glib */
unsigned int BLI_str_utf8_as_unicode(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
unsigned int BLI_str_utf8_as_unicode_step(const char *__restrict p,
                                          size_t p_len,
                                          size_t *__restrict index) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 3);
unsigned int BLI_str_utf8_as_unicode_step_or_error(
    const char *__restrict p, size_t p_len, size_t *__restrict index) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 3);

size_t BLI_str_utf8_from_unicode_len(unsigned int c) ATTR_WARN_UNUSED_RESULT;
size_t BLI_str_utf8_from_unicode(unsigned int c, char *outbuf, const size_t outbuf_len)
    ATTR_NONNULL(2);
size_t BLI_str_utf8_as_utf32(char32_t *__restrict dst_w,
                             const char *__restrict src_c,
                             const size_t maxncpy) ATTR_NONNULL(1, 2);
size_t BLI_str_utf32_as_utf8(char *__restrict dst,
                             const char32_t *__restrict src,
                             const size_t maxncpy) ATTR_NONNULL(1, 2);
size_t BLI_str_utf32_as_utf8_len(const char32_t *src) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

const char *BLI_str_find_prev_char_utf8(const char *p, const char *str_start)
    ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL ATTR_NONNULL(1, 2);
const char *BLI_str_find_next_char_utf8(const char *p, const char *str_end)
    ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL ATTR_NONNULL(1, 2);

size_t BLI_wstrlen_utf8(const wchar_t *src) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
size_t BLI_strlen_utf8_ex(const char *strc, size_t *r_len_bytes)
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;
size_t BLI_strlen_utf8(const char *strc) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
size_t BLI_strnlen_utf8_ex(const char *strc, const size_t maxlen, size_t *r_len_bytes)
    ATTR_NONNULL(1, 3);
size_t BLI_strnlen_utf8(const char *strc, const size_t maxlen)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
size_t BLI_strncpy_wchar_as_utf8(char *__restrict dst,
                                 const wchar_t *__restrict src,
                                 const size_t maxncpy) ATTR_NONNULL(1, 2);
size_t BLI_strncpy_wchar_from_utf8(wchar_t *__restrict dst,
                                   const char *__restrict src,
                                   const size_t maxncpy) ATTR_NONNULL(1, 2);

/* count columns that character/string occupies, based on wcwidth.c */
int BLI_wcwidth(char32_t ucs) ATTR_WARN_UNUSED_RESULT;
int BLI_wcswidth(const char32_t *pwcs, size_t n) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/* warning, can return -1 on bad chars */
int BLI_str_utf8_char_width(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLI_str_utf8_char_width_safe(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

size_t BLI_str_partition_utf8(const char *str,
                              const unsigned int delim[],
                              const char **sep,
                              const char **suf) ATTR_NONNULL(1, 2, 3, 4);
size_t BLI_str_rpartition_utf8(const char *str,
                               const unsigned int delim[],
                               const char **sep,
                               const char **suf) ATTR_NONNULL(1, 2, 3, 4);
size_t BLI_str_partition_ex_utf8(const char *str,
                                 const char *end,
                                 const unsigned int delim[],
                                 const char **sep,
                                 const char **suf,
                                 const bool from_right) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 3, 4, 5);

int BLI_str_utf8_offset_to_index(const char *str, int offset) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
int BLI_str_utf8_offset_from_index(const char *str, int index) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
int BLI_str_utf8_offset_to_column(const char *str, int offset) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
int BLI_str_utf8_offset_from_column(const char *str, int column) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);

#define BLI_UTF8_MAX 6       /* mem */
#define BLI_UTF8_WIDTH_MAX 2 /* columns */
#define BLI_UTF8_ERR ((unsigned int)-1)

/* -------------------------------------------------------------------- */
/** \name String Copy/Format Macros
 * Avoid repeating destination with `sizeof(..)`.
 * \note `ARRAY_SIZE` allows pointers on some platforms.
 * \{ */
#define STRNCPY_UTF8(dst, src) BLI_strncpy_utf8(dst, src, ARRAY_SIZE(dst))
#define STRNCPY_UTF8_RLEN(dst, src) BLI_strncpy_utf8_rlen(dst, src, ARRAY_SIZE(dst))
/** \} */

#ifdef __cplusplus
}
#endif
