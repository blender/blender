/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <stdarg.h>

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;

typedef bool (*UniquenameCheckCallback)(void *arg, const char *name);

/* ------------------------------------------------------------------------- */
/** \name String Replace
 * \{ */

/**
 * string with all instances of substr_old replaced with substr_new,
 * Returns a copy of the c-string \a str into a newly #MEM_mallocN'd
 * and returns it.
 *
 * \note A rather wasteful string-replacement utility, though this shall do for now.
 * Feel free to replace this with an even safe + nicer alternative
 *
 * \param str: The string to replace occurrences of substr_old in
 * \param substr_old: The text in the string to find and replace
 * \param substr_new: The text in the string to find and replace
 * \retval Returns the duplicated string
 */
char *BLI_string_replaceN(const char *__restrict str,
                          const char *__restrict substr_old,
                          const char *__restrict substr_new) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3) ATTR_MALLOC;

/**
 * In-place replace every \a src to \a dst in \a str.
 *
 * \param str: The string to operate on.
 * \param src: The character to replace.
 * \param dst: The character to replace with.
 */
void BLI_string_replace_char(char *str, char src, char dst) ATTR_NONNULL(1);

/**
 * Simple exact-match string replacement.
 *
 * \param replace_table: Array of source, destination pairs.
 *
 * \note Larger tables should use a hash table.
 */
bool BLI_string_replace_table_exact(char *string,
                                    size_t string_len,
                                    const char *replace_table[][2],
                                    int replace_table_len);

/**
 * Write `dst` into the range between `src_beg` & `src_end`,
 * resize within `string_maxncpy` limits, ensure null terminated.
 *
 * \return the length of `string`.
 */
size_t BLI_string_replace_range(
    char *string, size_t string_maxncpy, int src_beg, int src_end, const char *dst);

/** \} */

/* ------------------------------------------------------------------------- */
/** \name String Split
 * \{ */

/**
 * Looks for a numeric suffix preceded by `delim` character on the end of
 * name, puts preceding part into *left and value of suffix into *nr.
 * Returns the length of *left.
 *
 * Foo.001 -> "Foo", 1
 * Returning the length of "Foo"
 *
 * \param left: Where to return copy of part preceding `delim`.
 * \param nr: Where to return value of numeric suffix`.
 * \param name: String to split`.
 * \param delim: Delimiter character`.
 * \return Length of \a left.
 */
size_t BLI_string_split_name_number(const char *name, char delim, char *r_name_left, int *r_number)
    ATTR_NONNULL(1, 3, 4);
bool BLI_string_is_decimal(const char *string) ATTR_NONNULL(1);

/**
 * Based on `BLI_path_split_dir_file()` / `os.path.splitext()`,
 * `"a.b.c"` -> (`"a.b"`, `".c"`).
 */
void BLI_string_split_suffix(const char *string, size_t string_maxlen, char *r_body, char *r_suf)
    ATTR_NONNULL(1, 3, 4);
/**
 * `"a.b.c"` -> (`"a."`, `"b.c"`).
 */
void BLI_string_split_prefix(const char *string, size_t string_maxlen, char *r_pre, char *r_body)
    ATTR_NONNULL(1, 3, 4);

/** \} */

/**
 * A version of #BLI_string_join_array_by_sep_charN that takes a table array.
 * The new location of each string is written into this array.
 */
char *BLI_string_join_array_by_sep_char_with_tableN(char sep,
                                                    char *table[],
                                                    const char *strings[],
                                                    uint strings_num) ATTR_NONNULL(2, 3);

#define BLI_string_join_by_sep_char_with_tableN(sep, table, ...) \
  BLI_string_join_array_by_sep_char_with_tableN( \
      sep, table, ((const char *[]){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))

/**
 * Finds the best possible flipped (left/right) name.
 * For renaming; check for unique names afterwards.
 *
 * \param name_dst: flipped name,
 * assumed to be a pointer to a string of at least \a name_maxncpy size.
 * \param name_src: original name,
 * assumed to be a pointer to a string of at least \a name_maxncpy size.
 * \param strip_number: If set, remove number extensions.
 * \return The number of bytes written into \a name.
 */
size_t BLI_string_flip_side_name(char *name_dst,
                                 const char *name_src,
                                 bool strip_number,
                                 size_t name_dst_maxncpy) ATTR_NONNULL(1, 2);

/**
 * Ensures name is unique (according to criteria specified by caller in unique_check callback),
 * incrementing its numeric suffix as necessary. Returns true if name had to be adjusted.
 *
 * \param unique_check: Return true if name is not unique
 * \param arg: Additional arg to unique_check--meaning is up to caller
 * \param defname: To initialize name if latter is empty
 * \param delim: Delimits numeric suffix in name
 * \param name: Name to be ensured unique
 * \param name_maxncpy: Maximum length of name area
 * \return true if there if the name was changed
 */
bool BLI_uniquename_cb(UniquenameCheckCallback unique_check,
                       void *arg,
                       const char *defname,
                       char delim,
                       char *name,
                       size_t name_maxncpy) ATTR_NONNULL(1, 3, 5);
/**
 * Ensures that the specified block has a unique name within the containing list,
 * incrementing its numeric suffix as necessary. Returns true if name had to be adjusted.
 *
 * \param list: List containing the block
 * \param vlink: The block to check the name for
 * \param defname: To initialize block name if latter is empty
 * \param delim: Delimits numeric suffix in name
 * \param name_offset: Offset of name within block structure
 * \param name_maxncpy: Maximum length of name area
 */
bool BLI_uniquename(struct ListBase *list,
                    void *vlink,
                    const char *defname,
                    char delim,
                    int name_offset,
                    size_t name_maxncpy) ATTR_NONNULL(1, 3);

/* Expand array functions. */

size_t BLI_string_len_array(const char *strings[], uint strings_num) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

/* Intentionally no comma after `_BLI_STRING_ARGS_0` to allow it to be empty. */
#define _BLI_STRING_ARGS_1 _BLI_STRING_ARGS_0 const char *a
#define _BLI_STRING_ARGS_2 _BLI_STRING_ARGS_1, const char *b
#define _BLI_STRING_ARGS_3 _BLI_STRING_ARGS_2, const char *c
#define _BLI_STRING_ARGS_4 _BLI_STRING_ARGS_3, const char *d
#define _BLI_STRING_ARGS_5 _BLI_STRING_ARGS_4, const char *e
#define _BLI_STRING_ARGS_6 _BLI_STRING_ARGS_5, const char *f
#define _BLI_STRING_ARGS_7 _BLI_STRING_ARGS_6, const char *g
#define _BLI_STRING_ARGS_8 _BLI_STRING_ARGS_7, const char *h
#define _BLI_STRING_ARGS_9 _BLI_STRING_ARGS_8, const char *i
#define _BLI_STRING_ARGS_10 _BLI_STRING_ARGS_9, const char *j

/* ------------------------------------------------------------------------- */
/** \name Implement: `BLI_string_join(..)`
 * \{ */

#define _BLI_STRING_ARGS_0 char *__restrict dst, const size_t dst_len,

/**
 * Join strings, return the length of the resulting string.
 */
size_t BLI_string_join_array(char *result,
                             size_t result_maxncpy,
                             const char *strings[],
                             uint strings_num) ATTR_NONNULL();

#define BLI_string_join(...) VA_NARGS_CALL_OVERLOAD(_BLI_string_join_, __VA_ARGS__)

BLI_INLINE size_t _BLI_string_join_3(_BLI_STRING_ARGS_1) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_4(_BLI_STRING_ARGS_2) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_5(_BLI_STRING_ARGS_3) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_6(_BLI_STRING_ARGS_4) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_7(_BLI_STRING_ARGS_5) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_8(_BLI_STRING_ARGS_6) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_9(_BLI_STRING_ARGS_7) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_10(_BLI_STRING_ARGS_8) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_11(_BLI_STRING_ARGS_9) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_12(_BLI_STRING_ARGS_10) ATTR_NONNULL();

BLI_INLINE size_t _BLI_string_join_3(_BLI_STRING_ARGS_1)
{
  const char *string_array[] = {a};
  return BLI_string_join_array(dst, dst_len, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_4(_BLI_STRING_ARGS_2)
{
  const char *string_array[] = {a, b};
  return BLI_string_join_array(dst, dst_len, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_5(_BLI_STRING_ARGS_3)
{
  const char *string_array[] = {a, b, c};
  return BLI_string_join_array(dst, dst_len, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_6(_BLI_STRING_ARGS_4)
{
  const char *string_array[] = {a, b, c, d};
  return BLI_string_join_array(dst, dst_len, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_7(_BLI_STRING_ARGS_5)
{
  const char *string_array[] = {a, b, c, d, e};
  return BLI_string_join_array(dst, dst_len, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_8(_BLI_STRING_ARGS_6)
{
  const char *string_array[] = {a, b, c, d, e, f};
  return BLI_string_join_array(dst, dst_len, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_9(_BLI_STRING_ARGS_7)
{
  const char *string_array[] = {a, b, c, d, e, f, g};
  return BLI_string_join_array(dst, dst_len, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_10(_BLI_STRING_ARGS_8)
{
  const char *string_array[] = {a, b, c, d, e, f, g, h};
  return BLI_string_join_array(dst, dst_len, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_11(_BLI_STRING_ARGS_9)
{
  const char *string_array[] = {a, b, c, d, e, f, g, h, i};
  return BLI_string_join_array(dst, dst_len, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_12(_BLI_STRING_ARGS_10)
{
  const char *string_array[] = {a, b, c, d, e, f, g, h, i, j};
  return BLI_string_join_array(dst, dst_len, string_array, ARRAY_SIZE(string_array));
}

#undef _BLI_STRING_ARGS_0

/** \} */

/* ------------------------------------------------------------------------- */
/** \name Implement: `BLI_string_joinN(..)`
 * \{ */

/**
 * Join an array of strings into a newly allocated, null terminated string.
 */
char *BLI_string_join_arrayN(const char *strings[], uint strings_num) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

#define BLI_string_joinN(...) VA_NARGS_CALL_OVERLOAD(_BLI_string_joinN_, __VA_ARGS__)

#define _BLI_STRING_ARGS_0

BLI_INLINE char *_BLI_string_joinN_1(_BLI_STRING_ARGS_1) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_joinN_2(_BLI_STRING_ARGS_2) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_joinN_3(_BLI_STRING_ARGS_3) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_joinN_4(_BLI_STRING_ARGS_4) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_joinN_5(_BLI_STRING_ARGS_5) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_joinN_6(_BLI_STRING_ARGS_6) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_joinN_7(_BLI_STRING_ARGS_7) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_joinN_8(_BLI_STRING_ARGS_8) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_joinN_9(_BLI_STRING_ARGS_9) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_joinN_10(_BLI_STRING_ARGS_10) ATTR_NONNULL();

BLI_INLINE char *_BLI_string_joinN_1(_BLI_STRING_ARGS_1)
{
  const char *string_array[] = {a};
  return BLI_string_join_arrayN(string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_joinN_2(_BLI_STRING_ARGS_2)
{
  const char *string_array[] = {a, b};
  return BLI_string_join_arrayN(string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_joinN_3(_BLI_STRING_ARGS_3)
{
  const char *string_array[] = {a, b, c};
  return BLI_string_join_arrayN(string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_joinN_4(_BLI_STRING_ARGS_4)
{
  const char *string_array[] = {a, b, c, d};
  return BLI_string_join_arrayN(string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_joinN_5(_BLI_STRING_ARGS_5)
{
  const char *string_array[] = {a, b, c, d, e};
  return BLI_string_join_arrayN(string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_joinN_6(_BLI_STRING_ARGS_6)
{
  const char *string_array[] = {a, b, c, d, e, f};
  return BLI_string_join_arrayN(string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_joinN_7(_BLI_STRING_ARGS_7)
{
  const char *string_array[] = {a, b, c, d, e, f, g};
  return BLI_string_join_arrayN(string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_joinN_8(_BLI_STRING_ARGS_8)
{
  const char *string_array[] = {a, b, c, d, e, f, g, h};
  return BLI_string_join_arrayN(string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_joinN_9(_BLI_STRING_ARGS_9)
{
  const char *string_array[] = {a, b, c, d, e, f, g, h, i};
  return BLI_string_join_arrayN(string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_joinN_10(_BLI_STRING_ARGS_10)
{
  const char *string_array[] = {a, b, c, d, e, f, g, h, i, j};
  return BLI_string_join_arrayN(string_array, ARRAY_SIZE(string_array));
}

#undef _BLI_STRING_ARGS_0

/** \} */

/* ------------------------------------------------------------------------- */
/** \name Implement: `BLI_string_join_by_sep_char(..)`
 * \{ */

/**
 * A version of #BLI_string_join_array that takes a separator which can be any character
 * including '\0'.
 */
size_t BLI_string_join_array_by_sep_char(char *result,
                                         size_t result_maxncpy,
                                         char sep,
                                         const char *strings[],
                                         uint strings_num) ATTR_NONNULL();

#define BLI_string_join_by_sep_char(...) \
  VA_NARGS_CALL_OVERLOAD(_BLI_string_join_by_sep_char_, __VA_ARGS__)

#define _BLI_STRING_ARGS_0 char *__restrict dst, const size_t dst_len, const char sep,

BLI_INLINE size_t _BLI_string_join_by_sep_char_4(_BLI_STRING_ARGS_1) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_by_sep_char_5(_BLI_STRING_ARGS_2) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_by_sep_char_6(_BLI_STRING_ARGS_3) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_by_sep_char_7(_BLI_STRING_ARGS_4) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_by_sep_char_8(_BLI_STRING_ARGS_5) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_by_sep_char_9(_BLI_STRING_ARGS_6) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_by_sep_char_10(_BLI_STRING_ARGS_7) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_by_sep_char_11(_BLI_STRING_ARGS_8) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_by_sep_char_12(_BLI_STRING_ARGS_9) ATTR_NONNULL();
BLI_INLINE size_t _BLI_string_join_by_sep_char_13(_BLI_STRING_ARGS_10) ATTR_NONNULL();

BLI_INLINE size_t _BLI_string_join_by_sep_char_4(_BLI_STRING_ARGS_1)
{
  const char *string_array[] = {a};
  return BLI_string_join_array_by_sep_char(
      dst, dst_len, sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_by_sep_char_5(_BLI_STRING_ARGS_2)
{
  const char *string_array[] = {a, b};
  return BLI_string_join_array_by_sep_char(
      dst, dst_len, sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_by_sep_char_6(_BLI_STRING_ARGS_3)
{
  const char *string_array[] = {a, b, c};
  return BLI_string_join_array_by_sep_char(
      dst, dst_len, sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_by_sep_char_7(_BLI_STRING_ARGS_4)
{
  const char *string_array[] = {a, b, c, d};
  return BLI_string_join_array_by_sep_char(
      dst, dst_len, sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_by_sep_char_8(_BLI_STRING_ARGS_5)
{
  const char *string_array[] = {a, b, c, d, e};
  return BLI_string_join_array_by_sep_char(
      dst, dst_len, sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_by_sep_char_9(_BLI_STRING_ARGS_6)
{
  const char *string_array[] = {a, b, c, d, e, f};
  return BLI_string_join_array_by_sep_char(
      dst, dst_len, sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_by_sep_char_10(_BLI_STRING_ARGS_7)
{
  const char *string_array[] = {a, b, c, d, e, f, g};
  return BLI_string_join_array_by_sep_char(
      dst, dst_len, sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_by_sep_char_11(_BLI_STRING_ARGS_8)
{
  const char *string_array[] = {a, b, c, d, e, f, g, h};
  return BLI_string_join_array_by_sep_char(
      dst, dst_len, sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_by_sep_char_12(_BLI_STRING_ARGS_9)
{
  const char *string_array[] = {a, b, c, d, e, f, g, h, i};
  return BLI_string_join_array_by_sep_char(
      dst, dst_len, sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE size_t _BLI_string_join_by_sep_char_13(_BLI_STRING_ARGS_10)
{
  const char *string_array[] = {a, b, c, d, e, f, g, h, i, j};
  return BLI_string_join_array_by_sep_char(
      dst, dst_len, sep, string_array, ARRAY_SIZE(string_array));
}

#undef _BLI_STRING_ARGS_0

/** \} */

/* ------------------------------------------------------------------------- */
/** \name Implement: `BLI_string_join_by_sep_charN(..)`
 * \{ */

/**
 * A version of #BLI_string_join_by_sep_char that takes a separator which can be any character
 * including '\0'.
 */
char *BLI_string_join_array_by_sep_charN(char sep,
                                         const char *strings[],
                                         uint strings_num) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

#define BLI_string_join_by_sep_charN(...) \
  VA_NARGS_CALL_OVERLOAD(_BLI_string_join_by_sep_charN_, __VA_ARGS__)

#define _BLI_STRING_ARGS_0 const char sep,

BLI_INLINE char *_BLI_string_join_by_sep_charN_2(_BLI_STRING_ARGS_1) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_join_by_sep_charN_3(_BLI_STRING_ARGS_2) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_join_by_sep_charN_4(_BLI_STRING_ARGS_3) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_join_by_sep_charN_5(_BLI_STRING_ARGS_4) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_join_by_sep_charN_6(_BLI_STRING_ARGS_5) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_join_by_sep_charN_7(_BLI_STRING_ARGS_6) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_join_by_sep_charN_8(_BLI_STRING_ARGS_7) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_join_by_sep_charN_9(_BLI_STRING_ARGS_8) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_join_by_sep_charN_10(_BLI_STRING_ARGS_9) ATTR_NONNULL();
BLI_INLINE char *_BLI_string_join_by_sep_charN_11(_BLI_STRING_ARGS_10) ATTR_NONNULL();

BLI_INLINE char *_BLI_string_join_by_sep_charN_2(_BLI_STRING_ARGS_1)
{
  const char *string_array[] = {a};
  return BLI_string_join_array_by_sep_charN(sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_join_by_sep_charN_3(_BLI_STRING_ARGS_2)
{
  const char *string_array[] = {a, b};
  return BLI_string_join_array_by_sep_charN(sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_join_by_sep_charN_4(_BLI_STRING_ARGS_3)
{
  const char *string_array[] = {a, b, c};
  return BLI_string_join_array_by_sep_charN(sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_join_by_sep_charN_5(_BLI_STRING_ARGS_4)
{
  const char *string_array[] = {a, b, c, d};
  return BLI_string_join_array_by_sep_charN(sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_join_by_sep_charN_6(_BLI_STRING_ARGS_5)
{
  const char *string_array[] = {a, b, c, d, e};
  return BLI_string_join_array_by_sep_charN(sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_join_by_sep_charN_7(_BLI_STRING_ARGS_6)
{
  const char *string_array[] = {a, b, c, d, e, f};
  return BLI_string_join_array_by_sep_charN(sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_join_by_sep_charN_8(_BLI_STRING_ARGS_7)
{
  const char *string_array[] = {a, b, c, d, e, f, g};
  return BLI_string_join_array_by_sep_charN(sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_join_by_sep_charN_9(_BLI_STRING_ARGS_8)
{
  const char *string_array[] = {a, b, c, d, e, f, g, h};
  return BLI_string_join_array_by_sep_charN(sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_join_by_sep_charN_10(_BLI_STRING_ARGS_9)
{
  const char *string_array[] = {a, b, c, d, e, f, g, h, i};
  return BLI_string_join_array_by_sep_charN(sep, string_array, ARRAY_SIZE(string_array));
}
BLI_INLINE char *_BLI_string_join_by_sep_charN_11(_BLI_STRING_ARGS_10)
{
  const char *string_array[] = {a, b, c, d, e, f, g, h, i, j};
  return BLI_string_join_array_by_sep_charN(sep, string_array, ARRAY_SIZE(string_array));
}

/** \} */

#undef _BLI_STRING_ARGS_0

#ifdef __cplusplus
}
#endif
