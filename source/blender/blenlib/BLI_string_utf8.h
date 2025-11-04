/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <stdarg.h>

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

char *BLI_strncpy_utf8(char *__restrict dst, const char *__restrict src, size_t dst_maxncpy)
    ATTR_NONNULL(1, 2);
size_t BLI_strncpy_utf8_rlen(char *__restrict dst,
                             const char *__restrict src,
                             size_t dst_maxncpy) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);
/**
 * A version of #BLI_strncpy_utf8_rlen that doesn't null terminate the string.
 * \note Useful for C++ APIs that don't null terminate strings.
 */
size_t BLI_strncpy_utf8_rlen_unterminated(char *__restrict dst,
                                          const char *__restrict src,
                                          size_t dst_maxncpy);

/**
 * Find first UTF8 invalid byte in given \a str, of \a str_len bytes.
 *
 * \return the offset of the first invalid byte.
 */
ptrdiff_t BLI_str_utf8_invalid_byte(const char *str, size_t str_len) ATTR_NONNULL(1);
/**
 * Remove any invalid UTF8 byte (taking into account multi-bytes sequences).
 *
 * \param str: a null terminated string.
 * \param str_len: the result of `strlen(str)`.
 * \return number of stripped bytes.
 */
int BLI_str_utf8_invalid_strip(char *str, size_t str_len) ATTR_NONNULL(1);
/**
 * Substitute any invalid UTF8 byte with `substitute` (taking into account multi-bytes sequences).
 * The length of the string remains unchanged.
 *
 * \param str: a null terminated string.
 * \param str_len: the result of `strlen(str)`.
 * \return number of bytes replaced.
 */
int BLI_str_utf8_invalid_substitute(char *str, size_t str_len, const char substitute)
    ATTR_NONNULL(1);

/**
 * A utility for #BLI_str_utf8_invalid_substitute that returns `str` when it contains a
 * valid UTF8 string. Otherwise it is copied into `buf` with invalid byte sequences
 * substituted for `substitute`.
 *
 * \note This is intended for situations when the string is expected to be valid,
 * where copying and substituting values is typically not needed.
 */
[[nodiscard]] const char *BLI_str_utf8_invalid_substitute_if_needed(
    const char *str, size_t str_len, const char substitute, char *buf, const size_t buf_maxncpy)
    ATTR_NONNULL(1, 4);

/**
 * \return The size (in bytes) of a single UTF8 char.
 * \warning Can return -1 on bad chars.
 */
int BLI_str_utf8_size_or_error(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Use when we want to skip errors.
 */
int BLI_str_utf8_size_safe(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * \param p: a pointer to Unicode character encoded as UTF8
 *
 * Converts a sequence of bytes encoded as UTF8 to a Unicode character.
 * If \a p does not point to a valid UTF8 encoded character, results are
 * undefined. If you are not sure that the bytes are complete
 * valid Unicode characters, you should use g_utf8_get_char_validated()
 * instead.
 *
 * Return value: the resulting character
 */
unsigned int BLI_str_utf8_as_unicode_or_error(const char *p) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
unsigned int BLI_str_utf8_as_unicode_safe(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * UTF8 decoding that steps over the index.
 * When an error is encountered fall back to `LATIN1`, stepping over a single byte.
 *
 * \param p: The text to step over.
 * \param p_len: The length of `p`.
 * \param index: Index of `p` to step over.
 * \return the code-point `(p + *index)` if there is a decoding error.
 */
unsigned int BLI_str_utf8_as_unicode_step_safe(const char *__restrict p,
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
 * \param dst: output buffer, must have at least `dst_maxncpy` bytes of space.
 * If the length required by `c` exceeds `dst_maxncpy`,
 * the bytes available bytes will be zeroed and `dst_maxncpy` returned.
 *
 * Converts a single character to UTF8.
 *
 * \return number of bytes written.
 */
size_t BLI_str_utf8_from_unicode(unsigned int c, char *dst, size_t dst_maxncpy) ATTR_NONNULL(2);
size_t BLI_str_utf8_as_utf32(char32_t *__restrict dst_w,
                             const char *__restrict src_c,
                             size_t dst_w_maxncpy) ATTR_NONNULL(1, 2);
size_t BLI_str_utf32_as_utf8(char *__restrict dst,
                             const char32_t *__restrict src,
                             size_t dst_maxncpy) ATTR_NONNULL(1, 2);
/**
 * \return The UTF32 len in UTF8 with a clamped length.
 */
size_t BLI_str_utf32_as_utf8_len_ex(const char32_t *src, size_t src_maxlen) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
/**
 * \return The UTF32 len in UTF8.
 */
size_t BLI_str_utf32_as_utf8_len(const char32_t *src) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/**
 * BLI_str_find_prev_char_utf8:
 * \param p: pointer to some position within \a str
 * \param str_start: pointer to the beginning of a UTF8 encoded string
 *
 * Given a position \a p with a UTF8 encoded string \a str, find the start
 * of the previous UTF8 character starting before. \a p Returns \a str_start if no
 * UTF8 characters are present in \a str_start before \a p.
 *
 * \a p does not have to be at the beginning of a UTF8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 *
 * \return A pointer to the found character.
 */
const char *BLI_str_find_prev_char_utf8(const char *p, const char *str_start)
    ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL ATTR_NONNULL(1, 2);
/**
 * \param p: a pointer to a position within a UTF8 encoded string
 * \param str_end: a pointer to the byte following the end of the string.
 *
 * Finds the start of the next UTF8 character in the string after \a p
 *
 * \a p does not have to be at the beginning of a UTF8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 *
 * \return a pointer to the found character or a pointer to the null terminating character '\0'.
 */
const char *BLI_str_find_next_char_utf8(const char *p, const char *str_end)
    ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL ATTR_NONNULL(1, 2);

/**
 * \return the `wchar_t` length in UTF8.
 */
size_t BLI_wstrlen_utf8(const wchar_t *src) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
size_t BLI_strlen_utf8_ex(const char *strc, size_t *r_len_bytes)
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;
size_t BLI_strlen_utf8(const char *strc) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
size_t BLI_strnlen_utf8_ex(const char *strc, size_t strc_maxlen, size_t *r_len_bytes)
    ATTR_NONNULL(1, 3);
/**
 * \param strc: the string to measure the length.
 * \param strc_maxlen: the string length (in bytes)
 * \return the unicode length (not in bytes!)
 */
size_t BLI_strnlen_utf8(const char *strc, size_t strc_maxlen)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
size_t BLI_strncpy_wchar_as_utf8(char *__restrict dst,
                                 const wchar_t *__restrict src,
                                 size_t dst_maxncpy) ATTR_NONNULL(1, 2);
size_t BLI_strncpy_wchar_from_utf8(wchar_t *__restrict dst_w,
                                   const char *__restrict src_c,
                                   size_t dst_w_maxncpy) ATTR_NONNULL(1, 2);

/**
 * Portable replacement for `snprintf` that truncates partial UTF8 code-points.
 */
size_t BLI_snprintf_utf8(char *__restrict dst,
                         size_t dst_maxncpy,
                         const char *__restrict format,
                         ...) ATTR_NONNULL(1, 3) ATTR_PRINTF_FORMAT(3, 4);
/**
 * A version of #BLI_snprintf that truncates partial UTF8 code-points.
 *
 * \return The length of the string: `strlen(dst)`.
 */
size_t BLI_snprintf_utf8_rlen(char *__restrict dst,
                              size_t dst_maxncpy,
                              const char *__restrict format,
                              ...) ATTR_NONNULL(1, 3) ATTR_PRINTF_FORMAT(3, 4);

/**
 * Portable replacement for `vsnprintf` that truncates partial UTF8 code-points.
 */
size_t BLI_vsnprintf_utf8(char *__restrict dst,
                          size_t dst_maxncpy,
                          const char *__restrict format,
                          va_list arg) ATTR_PRINTF_FORMAT(3, 0);
/**
 * A version of #BLI_vsnprintf that truncates partial UTF8 code-points.
 * \return `strlen(dst)`
 */
size_t BLI_vsnprintf_utf8_rlen(char *__restrict dst,
                               size_t dst_maxncpy,
                               const char *__restrict format,
                               va_list arg) ATTR_PRINTF_FORMAT(3, 0);

/**
 * Count columns that character/string occupies (based on `wcwidth.co`).
 */
int BLI_wcwidth_or_error(char32_t ucs) ATTR_WARN_UNUSED_RESULT;
int BLI_wcwidth_safe(char32_t ucs) ATTR_WARN_UNUSED_RESULT;
int BLI_wcswidth_or_error(const char32_t *pwcs, size_t n) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/**
 * Return the uppercase of a 32-bit character or the character when no case change is needed.
 *
 * \note A 1:1 mapping doesn't account for multiple characters as part of conversion in some cases.
 */
char32_t BLI_str_utf32_char_to_upper(char32_t wc);
/**
 * Return the lowercase of a 32-bit character or the character when no case change is needed.
 *
 * \note A 1:1 mapping doesn't account for multiple characters as part of conversion in some cases.
 */
char32_t BLI_str_utf32_char_to_lower(char32_t wc);

bool BLI_str_utf32_char_is_breaking_space(char32_t codepoint);
bool BLI_str_utf32_char_is_optional_break_after(char32_t codepoint, char32_t codepoint_prev);
bool BLI_str_utf32_char_is_optional_break_before(char32_t codepoint, char32_t codepoint_prev);
bool BLI_str_utf32_char_is_terminal_punctuation(char32_t codepoint);

/**
 * \warning can return -1 on bad chars.
 */
int BLI_str_utf8_char_width_or_error(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLI_str_utf8_char_width_safe(const char *p) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

size_t BLI_str_partition_utf8(const char *str,
                              const unsigned int delim[],
                              const char **r_sep,
                              const char **r_suf) ATTR_NONNULL(1, 2, 3, 4);
size_t BLI_str_rpartition_utf8(const char *str,
                               const unsigned int delim[],
                               const char **r_sep,
                               const char **r_suf) ATTR_NONNULL(1, 2, 3, 4);
size_t BLI_str_partition_ex_utf8(const char *str,
                                 const char *end,
                                 const unsigned int delim[],
                                 const char **r_sep,
                                 const char **r_suf,
                                 bool from_right) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 3, 4, 5);

/**
 * Ensure that `str` has a null byte in the range of `[0..str_size]`, while not generating any
 * invalid UTF-8 code. The resulting `strlen(str)` is guaranteed to be less than `str_size`.
 *
 * \return true when `str` was truncated.
 */
bool BLI_str_utf8_truncate_at_size(char *str, const size_t str_size);

int BLI_str_utf8_offset_to_index(const char *str,
                                 size_t str_len,
                                 int offset_target) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Return the byte offset in `str` from `index_target`.
 * \param index_target: The unicode index, where multi-byte characters are counted once.
 * There is no need to clamp this value, the index is logically clamped to `BLI_strlen_utf8(str)`
 * or below.
 */
int BLI_str_utf8_offset_from_index(const char *str,
                                   size_t str_len,
                                   int index_target) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLI_str_utf8_offset_to_column(const char *str,
                                  size_t str_len,
                                  int offset_target) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLI_str_utf8_offset_from_column(const char *str,
                                    size_t str_len,
                                    int column_target) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLI_str_utf8_offset_to_column_with_tabs(const char *str,
                                            size_t str_len,
                                            int offset_target,
                                            int tab_width) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLI_str_utf8_offset_from_column_with_tabs(const char *str,
                                              size_t str_len,
                                              int column_target,
                                              int tab_width) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);

int BLI_str_utf8_column_count(const char *str, size_t str_len) ATTR_WARN_UNUSED_RESULT
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

#define STRNLEN_UTF8(str) BLI_strnlen_utf8(str, ARRAY_SIZE(str))

#define SNPRINTF_UTF8(dst, format, ...) \
  BLI_snprintf_utf8(dst, ARRAY_SIZE(dst), format, __VA_ARGS__)
#define SNPRINTF_UTF8_RLEN(dst, format, ...) \
  BLI_snprintf_utf8_rlen(dst, ARRAY_SIZE(dst), format, __VA_ARGS__)
#define VSNPRINTF_UTF8(dst, format, args) BLI_vsnprintf_utf8(dst, ARRAY_SIZE(dst), format, args)
#define VSNPRINTF_UTF8_RLEN(dst, format, args) \
  BLI_vsnprintf_utf8_rlen(dst, ARRAY_SIZE(dst), format, args)

/** \} */
