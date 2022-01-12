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
/**
 * Find first UTF-8 invalid byte in given \a str, of \a length bytes.
 *
 * \return the offset of the first invalid byte.
 */
ptrdiff_t BLI_str_utf8_invalid_byte(const char *str, size_t length) ATTR_NONNULL(1);
/**
 * Remove any invalid UTF-8 byte (taking into account multi-bytes sequence of course).
 *
 * \return number of stripped bytes.
 */
int BLI_str_utf8_invalid_strip(char *str, size_t length) ATTR_NONNULL(1);

/**
 * \return The size (in bytes) of a single UTF-8 char.
 * \warning Can return -1 on bad chars.
 */
int BLI_str_utf8_size(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Use when we want to skip errors.
 */
int BLI_str_utf8_size_safe(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
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
unsigned int BLI_str_utf8_as_unicode(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * UTF8 decoding that steps over the index (unless an error is encountered).
 *
 * \param p: The text to step over.
 * \param p_len: The length of `p`.
 * \param index: Index of `p` to step over.
 * \return the code-point `(p + *index)` if there is a decoding error.
 *
 * \note Falls back to `LATIN1` for text drawing.
 */
unsigned int BLI_str_utf8_as_unicode_step(const char *__restrict p,
                                          size_t p_len,
                                          size_t *__restrict index) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 3);
/**
 * UTF8 decoding that steps over the index (unless an error is encountered).
 *
 * \param p: The text to step over.
 * \param p_len: The length of `p`.
 * \param index: Index of `p` to step over.
 * \return the code-point or #BLI_UTF8_ERR if there is a decoding error.
 *
 * \note The behavior for clipped text (where `p_len` limits decoding trailing bytes)
 * must have the same behavior is encountering a nil byte,
 * so functions that only use the first part of a string has matching behavior to functions
 * that null terminate the text.
 */
unsigned int BLI_str_utf8_as_unicode_step_or_error(
    const char *__restrict p, size_t p_len, size_t *__restrict index) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 3);

size_t BLI_str_utf8_from_unicode_len(unsigned int c) ATTR_WARN_UNUSED_RESULT;
/**
 * BLI_str_utf8_from_unicode:
 *
 * \param c: a Unicode character code
 * \param outbuf: output buffer, must have at least `outbuf_len` bytes of space.
 * If the length required by `c` exceeds `outbuf_len`,
 * the bytes available bytes will be zeroed and `outbuf_len` returned.
 *
 * Converts a single character to UTF-8.
 *
 * \return number of bytes written.
 */
size_t BLI_str_utf8_from_unicode(unsigned int c, char *outbuf, const size_t outbuf_len)
    ATTR_NONNULL(2);
size_t BLI_str_utf8_as_utf32(char32_t *__restrict dst_w,
                             const char *__restrict src_c,
                             const size_t maxncpy) ATTR_NONNULL(1, 2);
size_t BLI_str_utf32_as_utf8(char *__restrict dst,
                             const char32_t *__restrict src,
                             const size_t maxncpy) ATTR_NONNULL(1, 2);
/**
 * \return The UTF-32 len in UTF-8.
 */
size_t BLI_str_utf32_as_utf8_len(const char32_t *src) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/**
 * BLI_str_find_prev_char_utf8:
 * \param p: pointer to some position within \a str
 * \param str_start: pointer to the beginning of a UTF-8 encoded string
 *
 * Given a position \a p with a UTF-8 encoded string \a str, find the start
 * of the previous UTF-8 character starting before. \a p Returns \a str_start if no
 * UTF-8 characters are present in \a str_start before \a p.
 *
 * \a p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 *
 * \return A pointer to the found character.
 */
const char *BLI_str_find_prev_char_utf8(const char *p, const char *str_start)
    ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL ATTR_NONNULL(1, 2);
/**
 * \param p: a pointer to a position within a UTF-8 encoded string
 * \param str_end: a pointer to the byte following the end of the string.
 *
 * Finds the start of the next UTF-8 character in the string after \a p
 *
 * \a p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 *
 * \return a pointer to the found character or a pointer to the null terminating character '\0'.
 */
const char *BLI_str_find_next_char_utf8(const char *p, const char *str_end)
    ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL ATTR_NONNULL(1, 2);

/**
 * \return the `wchar_t` length in UTF-8.
 */
size_t BLI_wstrlen_utf8(const wchar_t *src) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
size_t BLI_strlen_utf8_ex(const char *strc, size_t *r_len_bytes)
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;
size_t BLI_strlen_utf8(const char *strc) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
size_t BLI_strnlen_utf8_ex(const char *strc, const size_t maxlen, size_t *r_len_bytes)
    ATTR_NONNULL(1, 3);
/**
 * \param strc: the string to measure the length.
 * \param maxlen: the string length (in bytes)
 * \return the unicode length (not in bytes!)
 */
size_t BLI_strnlen_utf8(const char *strc, const size_t maxlen)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
size_t BLI_strncpy_wchar_as_utf8(char *__restrict dst,
                                 const wchar_t *__restrict src,
                                 const size_t maxncpy) ATTR_NONNULL(1, 2);
size_t BLI_strncpy_wchar_from_utf8(wchar_t *__restrict dst,
                                   const char *__restrict src,
                                   const size_t maxncpy) ATTR_NONNULL(1, 2);

/**
 * Count columns that character/string occupies (based on `wcwidth.co`).
 */
int BLI_wcwidth(char32_t ucs) ATTR_WARN_UNUSED_RESULT;
int BLI_wcswidth(const char32_t *pwcs, size_t n) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * \warning can return -1 on bad chars.
 */
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
                                 bool from_right) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 3, 4, 5);

int BLI_str_utf8_offset_to_index(const char *str, int offset) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
int BLI_str_utf8_offset_from_index(const char *str, int index) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
int BLI_str_utf8_offset_to_column(const char *str, int offset) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
int BLI_str_utf8_offset_from_column(const char *str, int column) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);

/** Size in bytes. */
#define BLI_UTF8_MAX 6
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
