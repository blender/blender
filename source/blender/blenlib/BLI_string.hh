/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <inttypes.h>
#include <stdarg.h>
#include <string>

#include "BLI_compiler_attrs.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.hh"

namespace blender {

/* Buffer size of maximum `uint64` plus commas and terminator. */
#define BLI_STR_FORMAT_UINT64_GROUPED_SIZE 27

/* Buffer size of maximum `int64` plus commas and terminator. */
#define BLI_STR_FORMAT_INT64_GROUPED_SIZE 28

/* Buffer size of maximum `int32` with commas and terminator. */
#define BLI_STR_FORMAT_INT32_GROUPED_SIZE 15

/* Buffer size of maximum `int64` formatted as byte size (with GiB for example). */
#define BLI_STR_FORMAT_INT64_BYTE_UNIT_SIZE 15

/* Buffer size of maximum `int64` formatted as byte size, minimum length (".1G" for example). */
#define BLI_STR_FORMAT_INT64_BYTE_UNIT_COMPACT_SIZE 5

/* Buffer size of maximum `int32` formatted as compact decimal size ("15.6M" for example). */
#define BLI_STR_FORMAT_INT32_DECIMAL_UNIT_SIZE 7

/* Buffer size of maximum `int32` formatted as very short decimal size ("15B" for example). */
#define BLI_STR_FORMAT_INT32_INTEGER_UNIT_SIZE 5

/**
 * Duplicates the first \a len bytes of the C-string \a str
 * into a newly mallocN'd string and returns it. \a str
 * is assumed to be at least len bytes long.
 *
 * \param str: The string to be duplicated
 * \param len: The number of bytes to duplicate
 * \retval Returns the duplicated string
 */
char *BLI_strdupn(const char *str, size_t len) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/**
 * Duplicates the C-string \a str into a newly mallocN'd
 * string and returns it.
 *
 * \param str: The string to be duplicated
 * \retval Returns the duplicated string
 */
char *BLI_strdup(const char *str) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1) ATTR_MALLOC;

/**
 * Duplicates the C-string \a str into a newly mallocN'd
 * string and returns it.
 *
 * \param str: The string to be duplicated, can be null
 * \retval Returns the duplicated string or null if \a str is null
 */
char *BLI_strdup_null(const char *str) ATTR_WARN_UNUSED_RESULT ATTR_MALLOC;

/**
 * Appends the two strings, and returns new mallocN'ed string
 * \param str1: first string for copy
 * \param str2: second string for append
 * \retval Returns dst
 */
char *BLI_strdupcat(const char *__restrict str1,
                    const char *__restrict str2) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2) ATTR_MALLOC;

/**
 * Like `strncpy` but ensures dst is always `\0` terminated.
 *
 * \param dst: Destination for copy.
 * \param src: Source string to copy.
 * \param dst_maxncpy: Maximum number of characters to copy (generally the size of dst).
 * \retval Returns dst
 *
 * \note #STRNCPY takes an array or a #MutableCStr, so the size can be omitted.
 */
char *BLI_strncpy(char *__restrict dst, const char *__restrict src, size_t dst_maxncpy)
    ATTR_NONNULL(1, 2);

/**
 * Like #BLI_strncpy but ensures `dst` is padded by `pad` on both sides,
 * `pad` is only added where `src` doesn't already start or end with it.
 *
 * When `src` is empty, `dst` is empty (no padding is added).
 * When `src` doesn't fit, it's truncated to make room for the trailing `pad`.
 * When `dst` is too small to fit a character between both pads,
 * the result is only the leading `pad` (empty when the size of `dst` is 1).
 *
 * \param dst: Destination for copy (its size includes the nil terminator).
 * \param src: Source string to copy.
 * \param pad: The character to use for padding.
 * \retval Returns `dst`.
 */
char *BLI_strncpy_ensure_pad(MutableCStr dst, const char *__restrict src, char pad)
    ATTR_NONNULL(2);

/**
 * Like `strncpy` but ensures dst is always `\0` terminated.
 *
 * \note This is a duplicate of #BLI_strncpy that returns bytes copied.
 * And is a drop in replacement for `snprintf(str, sizeof(str), "%s", arg);`
 * \note #STRNCPY_RLEN takes an array or a #MutableCStr, so the size can be omitted.
 *
 * \param dst: Destination for copy
 * \param src: Source string to copy
 * \param dst_maxncpy: Maximum number of characters to copy (generally the size of dst).
 * \retval The number of bytes copied (The only difference from #BLI_strncpy).
 */
size_t BLI_strncpy_rlen(char *__restrict dst,
                        const char *__restrict src,
                        size_t dst_maxncpy) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);

char *BLI_strncat(MutableCStr dst, const char *__restrict src) ATTR_NONNULL(2);

/**
 * A version of `strchr` that returns the end of the string (point to `\0`)
 * if the character is not found.
 *
 * Useful for stepping over newlines up until the last line.
 */
const char *BLI_strchr_or_end(const char *str,
                              char ch) ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL
    ATTR_NONNULL(1);

/**
 * Return the range of the quoted string (excluding quotes) `str` after `prefix`.
 *
 * A version of #BLI_str_quoted_substrN that calculates the range
 * instead of un-escaping and allocating the result.
 *
 * \param str: String potentially including `prefix`.
 * \param prefix: Quoted string prefix.
 * \param r_start: The start of the quoted string (after the first quote).
 * \param r_end: The end of the quoted string (before the last quote).
 * \return True when a quoted string range could be found after `prefix`.
 */
bool BLI_str_quoted_substr_range(const char *__restrict str,
                                 const char *__restrict prefix,
                                 int *__restrict r_start,
                                 int *__restrict r_end) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3, 4);
#if 0 /* UNUSED */
char *BLI_str_quoted_substrN(const char *__restrict str,
                             const char *__restrict prefix) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2) ATTR_MALLOC;
#endif
/**
 * Fills \a result with text within "" that appear after some the contents of \a prefix.
 * i.e. for string `pose["apples"]` with prefix `pose[`, it will return `apples`.
 *
 * \param str: is the entire string to chop.
 * \param prefix: is the part of the string to step over.
 * \param result: The buffer to fill.
 * \return True if the prefix was found and the entire quoted string was copied into result.
 *
 * Assume that the strings returned must be freed afterwards,
 * and that the inputs will contain data we want.
 */
bool BLI_str_quoted_substr(const char *__restrict str,
                           const char *__restrict prefix,
                           MutableCStr result);

/**
 * Portable replacement for `snprintf`.
 *
 * \note #SNPRINTF takes an array or a #MutableCStr, so the size can be omitted.
 */
size_t BLI_snprintf(char *__restrict dst, size_t dst_maxncpy, const char *__restrict format, ...)
    ATTR_NONNULL(1, 3) ATTR_PRINTF_FORMAT(3, 4);
/**
 * A version of #BLI_snprintf that returns `strlen(dst)`
 *
 * \note #SNPRINTF_RLEN takes an array or a #MutableCStr, so the size can be omitted.
 */
size_t BLI_snprintf_rlen(char *__restrict dst,
                         size_t dst_maxncpy,
                         const char *__restrict format,
                         ...) ATTR_NONNULL(1, 3) ATTR_PRINTF_FORMAT(3, 4);

/**
 * Portable replacement for `vsnprintf`.
 *
 * \note #VSNPRINTF takes an array or a #MutableCStr, so the size can be omitted.
 */
size_t BLI_vsnprintf(char *__restrict dst,
                     size_t dst_maxncpy,
                     const char *__restrict format,
                     va_list arg) ATTR_PRINTF_FORMAT(3, 0);
/**
 * A version of #BLI_vsnprintf that returns `strlen(dst)`
 *
 * \note #VSNPRINTF_RLEN takes an array or a #MutableCStr, so the size can be omitted.
 */
size_t BLI_vsnprintf_rlen(char *__restrict dst,
                          size_t dst_maxncpy,
                          const char *__restrict format,
                          va_list arg) ATTR_PRINTF_FORMAT(3, 0);

char *BLI_sprintfN_with_buffer(MutableCStr fixed_buf,
                               size_t *result_len,
                               const char *__restrict format,
                               ...) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2, 3)
    ATTR_PRINTF_FORMAT(3, 4);
char *BLI_vsprintfN_with_buffer(MutableCStr fixed_buf,
                                size_t *result_len,
                                const char *__restrict format,
                                va_list args) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2, 3)
    ATTR_PRINTF_FORMAT(3, 0);

/**
 * Print formatted string into a newly #MEM_new_uninitialized'd string
 * and return it.
 */
char *BLI_sprintfN(const char *__restrict format, ...) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1) ATTR_MALLOC ATTR_PRINTF_FORMAT(1, 2);
/** A version of #BLI_sprintfN that takes a #va_list. */
char *BLI_vsprintfN(const char *__restrict format, va_list args) ATTR_NONNULL(1) ATTR_MALLOC
    ATTR_PRINTF_FORMAT(1, 0);

/**
 * This roughly matches C and Python's string escaping with double quotes - `"`.
 *
 * Since every character may need escaping,
 * it's common to create a buffer twice as large as the input.
 *
 * \param dst: The destination string, typically `(strlen(src) * 2) + 1` in size.
 * \param src: The un-escaped source string.
 *
 * \note This is used for creating animation paths in blend files.
 */
size_t BLI_str_escape(MutableCStr dst, StringRef src);

/** Same as above, but returns an std::string. */
std::string BLI_str_escape(StringRef str);

/**
 * This roughly matches C and Python's string escaping with double quotes - `"`.
 *
 * The destination will never be larger than the source, it will either be the same
 * or up to half when all characters are escaped.
 *
 * \param dst: The destination string, at least the size of `strlen(src) + 1`.
 * \param src: The escaped source string.
 * \param src_len: The maximum number of bytes to read from `src`.
 * \param r_is_complete: Set to false when the result doesn't fit into `dst`.
 */
size_t BLI_str_unescape_ex(MutableCStr dst,
                           const char *__restrict src,
                           size_t src_len,
                           /* Additional arguments. */
                           bool *r_is_complete) ATTR_NONNULL(2, 4);
/**
 * See #BLI_str_unescape_ex docstring.
 *
 * This function makes the assumption that `dst` always has
 * at least `src_len + 1` bytes available.
 *
 * Use #BLI_str_unescape_ex if `dst` has a smaller fixed size.
 *
 * \note This is used for parsing animation paths in blend files (runs often).
 */
size_t BLI_str_unescape(char *__restrict dst, const char *__restrict src, size_t src_len)
    ATTR_NONNULL(1, 2);

/**
 * Find the first un-escaped quote in the string (to find the end of the string).
 *
 * \param str: Typically this is the first character in a quoted string.
 * Where the character before `*str` would be `"`.
 *
 * \return The pointer to the first un-escaped quote.
 */
const char *BLI_str_escape_find_quote(const char *str) ATTR_NONNULL(1);

/**
 * Format integers with decimal grouping.
 * 1000 -> 1,000
 *
 * \param dst: The resulting string.
 * \param num: Number to format.
 * \return The length of \a dst
 */
size_t BLI_str_format_int_grouped(char dst[BLI_STR_FORMAT_INT32_GROUPED_SIZE], int num)
    ATTR_NONNULL(1);
/**
 * Format uint64_t with decimal grouping.
 * 1000 -> 1,000
 *
 * \param dst: The resulting string.
 * \param num: Number to format.
 * \return The length of \a dst.
 */
size_t BLI_str_format_uint64_grouped(char dst[BLI_STR_FORMAT_UINT64_GROUPED_SIZE], uint64_t num)
    ATTR_NONNULL(1);
size_t BLI_str_format_int64_grouped(char dst[BLI_STR_FORMAT_INT64_GROUPED_SIZE], int64_t num)
    ATTR_NONNULL(1);
/**
 * Format a size in bytes using decimal (KB, MB, ...) or binary (KiB, MiB, ...) units.
 * 1000 -> 1 KB
 * Number of decimal places grows with the used unit (e.g. 1.5 MB, 1.55 GB, 1.545 TB).
 *
 * \param dst: The resulting string.
 * Dimension of 15 to support the longest result (`-8191.8472 PiB`).
 * \param bytes: Number to format.
 * \param base_10: Use decimal units (KB, MB, ...) or binary units (KiB, MiB, ...).
 */
void BLI_str_format_byte_unit(char dst[BLI_STR_FORMAT_INT64_BYTE_UNIT_SIZE],
                              int64_t bytes,
                              bool base_10) ATTR_NONNULL(1);

/**
 * Format a size in bytes using decimal or binary units in compact (max 4 chars) format.
 *
 * It shows a lower bound instead of rounding the number, except with binary units where:
 * - 1000-1023 of a unit is shown as 1 of the next unit.
 * - 100-999 of a unit is divided by 1000 (not 1024), so 500 KiB is shown as `.5M`.
 *
 * Negative values are shown without a sign for historic reasons:
 * this has only been used for file sizes & the buffer has no room for one.
 *
 * 1 -> 1B
 * 15 -> 15B
 * 155 -> 155B
 * 1555 -> 1K
 * 15555 -> 15K
 * 155555 -> .1M
 * 1555555 -> 1M
 * 15555555 -> 15M
 * 155555555 -> .1G
 * 1000000000 -> 1G
 * ...
 *
 * \param dst: The resulting string.
 * Dimension of 5 to support the longest result (`999B` for example).
 * \param bytes: Number to format.
 * \param base_10: Use decimal units (KB, MB, ...) or binary units (KiB, MiB, ...).
 */
void BLI_str_format_byte_unit_compact(char dst[BLI_STR_FORMAT_INT64_BYTE_UNIT_COMPACT_SIZE],
                                      int64_t bytes,
                                      bool base_10) ATTR_NONNULL(1);

/**
 * Format a count to up to 6 places (plus `\0` terminator) string using long number
 * names abbreviations. Used to produce a compact representation of large numbers.
 *
 * 1 -> 1
 * 15 -> 15
 * 155 -> 155
 * 1555 -> 1.6K
 * 15555 -> 15.6K
 * 155555 -> 156K
 * 1555555 -> 1.6M
 * 15555555 -> 15.6M
 * 155555555 -> 156M
 * 1000000000 -> 1.0B
 * ...
 *
 * Length of 7 is the maximum of the resulting string, for example, `-15.5K\0`.
 */
void BLI_str_format_decimal_unit(char dst[BLI_STR_FORMAT_INT32_DECIMAL_UNIT_SIZE],
                                 int number_to_format) ATTR_NONNULL(1);
/**
 * Format a count to up to 3 places (plus minus sign, plus '\0' terminator) string using long
 * number names abbreviations. Used to produce a compact representation of large numbers as
 * integers.
 *
 * It shows a lower bound instead of rounding the number.
 *
 * 1 -> 1
 * 15 -> 15
 * 155 -> 155
 * 1555 -> 1K
 * 15555 -> 15K
 * 155555 -> .1M
 * 1555555 -> 1M
 * 15555555 -> 15M
 * 155555555 -> .1B
 * 1000000000 -> 1B
 * ...
 *
 * Length of 5 is the maximum of the resulting string, for example, `-15K\0`.
 */
void BLI_str_format_integer_unit(char dst[BLI_STR_FORMAT_INT32_INTEGER_UNIT_SIZE],
                                 int number_to_format) ATTR_NONNULL(1);
/**
 * Compare two strings without regard to case.
 *
 * \retval True if the strings are equal, false otherwise.
 */
int BLI_strcaseeq(const char *a, const char *b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);
/**
 * Portable replacement for `strcasestr` (not available in MSVC)
 */
char *BLI_strcasestr(const char *s, const char *find) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);
/**
 * Variation of #BLI_strcasestr with string length limited to \a len
 */
char *BLI_strncasestr(const char *s, const char *find, size_t len) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);
int BLI_strcasecmp(const char *s1, const char *s2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);
int BLI_strncasecmp(const char *s1, const char *s2, size_t len) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);
/**
 * Case insensitive, *natural* string comparison,
 * keeping numbers in order.
 */
int BLI_strcasecmp_natural(const char *s1, const char *s2) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);
/**
 * Like `strcmp`, but will ignore any heading/trailing pad char for comparison.
 * So e.g. if pad is `*`, `*world` and `world*` will compare equal.
 */
int BLI_strcmp_ignore_pad(const char *str1, const char *str2, char pad) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);

/**
 * Determine the length of a fixed-size string.
 *
 * \return The string length that doesn't exceed `maxlen`.
 * The equivalent of `min(strlen(str), maxlen)` that prevents a buffer overflow
 * when `str` isn't null terminated before `maxlen`.
 *
 * \note #STRNLEN takes an array or a span, so the size can be omitted.
 */
size_t BLI_strnlen(const char *str, size_t maxlen) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/**
 * String case conversion, not affected by locale.
 */

void BLI_str_tolower_ascii(char *str, size_t len) ATTR_NONNULL(1);
void BLI_str_toupper_ascii(char *str, size_t len) ATTR_NONNULL(1);

char BLI_tolower_ascii(const char c) ATTR_WARN_UNUSED_RESULT;
char BLI_toupper_ascii(const char c) ATTR_WARN_UNUSED_RESULT;

/**
 * Strip white-space from end of the string.
 */
void BLI_str_rstrip(char *str) ATTR_NONNULL(1);
/**
 * Strip trailing zeros from a float, eg:
 * - 0.0000 -> 0.0
 * - 2.0010 -> 2.001
 *
 * \param str:
 * \param pad:
 * \return The number of zeros stripped.
 */
int BLI_str_rstrip_float_zero(char *str, char pad) ATTR_NONNULL(1);

/**
 * Strip trailing digits.
 * - ABC123 -> ABC
 *
 * \param str:
 * \return The number of digits stripped.
 */
int BLI_str_rstrip_digits(char *str) ATTR_NONNULL();

/**
 * Return index of a string in a string array.
 *
 * \param str: The string to find.
 * \param str_array: Array of strings.
 * \param str_array_len: The length of the array, or -1 for a NULL-terminated array.
 * \return The index of str in str_array or -1.
 */
int BLI_str_index_in_array_n(const char *__restrict str,
                             const char **__restrict str_array,
                             int str_array_len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Return index of a string in a string array.
 *
 * \param str: The string to find.
 * \param str_array: Array of strings, (must be NULL-terminated).
 * \return The index of str in str_array or -1.
 */
int BLI_str_index_in_array(const char *__restrict str,
                           const char **__restrict str_array) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);

/**
 * Find if a string starts with another string.
 *
 * \param str: The string to search within.
 * \param start: The string we look for at the start.
 * \return If str starts with start.
 */
bool BLI_str_startswith(const char *__restrict str,
                        const char *__restrict start) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);
/**
 * Find if a string ends with another string.
 *
 * \param str: The string to search within.
 * \param end: The string we look for at the end.
 * \return If `str` ends with `end`.
 */
bool BLI_str_endswith(const char *__restrict str, const char *__restrict end) ATTR_NONNULL(1, 2);
bool BLI_strn_endswith(const char *__restrict str, const char *__restrict end, size_t str_len)
    ATTR_NONNULL(1, 2);

/**
 * Find the first char matching one of the chars in \a delim, from left.
 *
 * \param str: The string to search within.
 * \param delim: The set of delimiters to search for, as unicode values.
 * \param sep: Return value, set to the first delimiter found (or NULL if none found).
 * \param suf: Return value, set to next char after the first delimiter found
 * (or NULL if none found).
 * \return The length of the prefix (i.e. *sep - str).
 */
size_t BLI_str_partition(const char *str, const char delim[], const char **sep, const char **suf)
    ATTR_NONNULL(1, 2, 3, 4);
/**
 * Find the first char matching one of the chars in \a delim, from right.
 *
 * \param str: The string to search within.
 * \param delim: The set of delimiters to search for, as unicode values.
 * \param sep: Return value, set to the first delimiter found (or NULL if none found).
 * \param suf: Return value, set to next char after the first delimiter found
 * (or NULL if none found).
 * \return The length of the prefix (i.e. *sep - str).
 */
size_t BLI_str_rpartition(const char *str, const char delim[], const char **sep, const char **suf)
    ATTR_NONNULL(1, 2, 3, 4);
/**
 * Find the first char matching one of the chars in \a delim, either from left or right.
 *
 * \param str: The string to search within.
 * \param end: If non-NULL, the right delimiter of the string.
 * \param delim: The set of delimiters to search for, as unicode values.
 * \param sep: Return value, set to the first delimiter found (or NULL if none found).
 * \param suf: Return value, set to next char after the first delimiter found
 * (or NULL if none found).
 * \param from_right: If %true, search from the right of \a str, else, search from its left.
 * \return The length of the prefix (i.e. *sep - str).
 */
size_t BLI_str_partition_ex(const char *str,
                            const char *end,
                            const char delim[],
                            const char **sep,
                            const char **suf,
                            bool from_right) ATTR_NONNULL(1, 3, 4, 5);

int BLI_string_max_possible_word_count(int str_len) ATTR_WARN_UNUSED_RESULT;
bool BLI_string_has_word_prefix(const char *haystack,
                                const char *needle,
                                size_t needle_len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);
bool BLI_string_all_words_matched(const char *name,
                                  const char *str,
                                  int (*words)[2],
                                  int words_len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3);

/**
 * Find the ranges needed to split \a str into its individual words.
 *
 * \param str: The string to search for words.
 * \param str_maxlen: Size of the string to search (ignored when larger than `strlen(str)`).
 * \param delim: Character to use as a delimiter.
 * \param r_words: Info about the words found. Set to [index, len] pairs.
 * \param words_max: Max number of words to find
 * \return The number of words found in \a str
 */
int BLI_string_find_split_words(const char *str,
                                size_t str_maxlen,
                                char delim,
                                int r_words[][2],
                                int words_max) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 4);

/**
 * A version of `STR_ELEM(..)` that treats `haystack` as multiple elements split by `delim`.
 * Return true when `needle` is found in `haystack`.
 *
 * \param haystack: The string to search in.
 * \param delim: The delimiter which divides haystack.
 * \param needle: The string to search for
 * (must not contain `delim` or the result will never be true).
 *
 * The following guarantees are made:
 * - Only an exact match returns true, requiring the strings length and case be match.
 * - Successive delimiters are supported.
 * - An empty needle will match against:
 *   - A blank string.
 *   - Delimiters at the beginning or end of the string
 *   - Two successive delimiters.
 */
bool BLI_string_elem_split_by_delim(const char *haystack,
                                    const char delim,
                                    const char *needle) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 3);

/* -------------------------------------------------------------------- */
/** \name String Copy/Format Wrappers
 * Avoid repeating destination with `sizeof(..)`.
 * \note The destination may be an array, a span or a container.
 * \{ */

#define SNPRINTF(dst, format, ...) \
  BLI_snprintf(SPAN_OR_ARRAY_DATA(dst), SPAN_OR_ARRAY_SIZE(dst), format, __VA_ARGS__)
#define SNPRINTF_RLEN(dst, format, ...) \
  BLI_snprintf_rlen(SPAN_OR_ARRAY_DATA(dst), SPAN_OR_ARRAY_SIZE(dst), format, __VA_ARGS__)
#define VSNPRINTF(dst, format, args) \
  BLI_vsnprintf(SPAN_OR_ARRAY_DATA(dst), SPAN_OR_ARRAY_SIZE(dst), format, args)
#define VSNPRINTF_RLEN(dst, format, args) \
  BLI_vsnprintf_rlen(SPAN_OR_ARRAY_DATA(dst), SPAN_OR_ARRAY_SIZE(dst), format, args)
#define STR_CONCAT(dst, len, suffix) \
  len += BLI_strncpy_rlen(SPAN_OR_ARRAY_DATA(dst) + len, suffix, SPAN_OR_ARRAY_SIZE(dst) - len)
#define STR_CONCATF(dst, len, format, ...) \
  len += BLI_snprintf_rlen( \
      SPAN_OR_ARRAY_DATA(dst) + len, SPAN_OR_ARRAY_SIZE(dst) - len, format, __VA_ARGS__)

/** Copy `src` into `dst` (an array or a #MutableCStr), see #BLI_strncpy. */
inline char *STRNCPY(MutableCStr dst, const char *src)
{
  return BLI_strncpy(dst.data(), src, dst.size());
}
/** A version of #STRNCPY that returns the length of the copied string. */
inline size_t STRNCPY_RLEN(MutableCStr dst, const char *src)
{
  return BLI_strncpy_rlen(dst.data(), src, dst.size());
}

/** The length of a string, not exceeding its buffer (an array or a span). */
[[nodiscard]] inline size_t STRNLEN(Span<char> str)
{
  return BLI_strnlen(str.data(), str.size());
}
/** Non-const arrays only convert to a #MutableCStr. */
[[nodiscard]] inline size_t STRNLEN(MutableCStr str)
{
  return BLI_strnlen(str.data(), str.size());
}

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

/* -------------------------------------------------------------------- */
/** \name String Debugging
 * \{ */
#ifdef WITH_STRSIZE_DEBUG
template<typename T> inline void BLI_string_debug_size(T *str, const size_t str_maxncpy)
{
  memset(str, 0xff, sizeof(T) * str_maxncpy);
}
/**
 * Fill `str` with a non-nil value after the trailing nil character,
 * use to ensure buffer sizes passed to string functions are correct.
 */
void BLI_string_debug_size_after_nil(char *str, size_t str_maxncpy);
#else
template<typename T> inline void BLI_string_debug_size(T * /*str*/, const size_t /*str_maxncpy*/)
{
}
inline void BLI_string_debug_size_after_nil(char * /*str*/, const size_t /*str_maxncpy*/) {}
#endif /* !WITH_STRSIZE_DEBUG */

inline void BLI_string_debug_size(MutableCStr str)
{
  BLI_string_debug_size(str.data(), str.size());
}
inline void BLI_string_debug_size_after_nil(MutableCStr str)
{
  BLI_string_debug_size_after_nil(str.data(), str.size());
}

/** \} */

}  // namespace blender
