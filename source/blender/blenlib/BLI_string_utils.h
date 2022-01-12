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
 * The Original Code is Copyright (C) 2017 by the Blender Foundation.
 * All rights reserved.
 */

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
size_t BLI_split_name_num(char *left, int *nr, const char *name, char delim);
bool BLI_string_is_decimal(const char *string) ATTR_NONNULL();

/**
 * Based on `BLI_split_dirfile()` / `os.path.splitext()`,
 * `"a.b.c"` -> (`"a.b"`, `".c"`).
 */
void BLI_string_split_suffix(const char *string, char *r_body, char *r_suf, const size_t str_len);
/**
 * `"a.b.c"` -> (`"a."`, `"b.c"`).
 */
void BLI_string_split_prefix(const char *string, char *r_pre, char *r_body, const size_t str_len);

/**
 * Join strings, return newly allocated string.
 */
char *BLI_string_join_array(char *result,
                            size_t result_len,
                            const char *strings[],
                            uint strings_len) ATTR_NONNULL();
/**
 * A version of #BLI_string_join that takes a separator which can be any character including '\0'.
 */
char *BLI_string_join_array_by_sep_char(char *result,
                                        size_t result_len,
                                        char sep,
                                        const char *strings[],
                                        uint strings_len) ATTR_NONNULL();

/**
 * Join an array of strings into a newly allocated, null terminated string.
 */
char *BLI_string_join_arrayN(const char *strings[], uint strings_len) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * A version of #BLI_string_joinN that takes a separator which can be any character including '\0'.
 */
char *BLI_string_join_array_by_sep_charN(char sep,
                                         const char *strings[],
                                         uint strings_len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * A version of #BLI_string_join_array_by_sep_charN that takes a table array.
 * The new location of each string is written into this array.
 */
char *BLI_string_join_array_by_sep_char_with_tableN(char sep,
                                                    char *table[],
                                                    const char *strings[],
                                                    uint strings_len) ATTR_NONNULL();
/**
 * Take multiple arguments, pass as (array, length).
 */
#define BLI_string_join(result, result_len, ...) \
  BLI_string_join_array( \
      result, result_len, ((const char *[]){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))
#define BLI_string_joinN(...) \
  BLI_string_join_arrayN(((const char *[]){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))
#define BLI_string_join_by_sep_charN(sep, ...) \
  BLI_string_join_array_by_sep_charN( \
      sep, ((const char *[]){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))
#define BLI_string_join_by_sep_char_with_tableN(sep, table, ...) \
  BLI_string_join_array_by_sep_char_with_tableN( \
      sep, table, ((const char *[]){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))

/**
 * Finds the best possible flipped (left/right) name.
 * For renaming; check for unique names afterwards.
 *
 * \param r_name: flipped name,
 * assumed to be a pointer to a string of at least \a name_len size.
 * \param from_name: original name,
 * assumed to be a pointer to a string of at least \a name_len size.
 * \param strip_number: If set, remove number extensions.
 * \return The number of bytes written into \a r_name.
 */
size_t BLI_string_flip_side_name(char *r_name,
                                 const char *from_name,
                                 bool strip_number,
                                 const size_t name_len);

/**
 * Ensures name is unique (according to criteria specified by caller in unique_check callback),
 * incrementing its numeric suffix as necessary. Returns true if name had to be adjusted.
 *
 * \param unique_check: Return true if name is not unique
 * \param arg: Additional arg to unique_check--meaning is up to caller
 * \param defname: To initialize name if latter is empty
 * \param delim: Delimits numeric suffix in name
 * \param name: Name to be ensured unique
 * \param name_len: Maximum length of name area
 * \return true if there if the name was changed
 */
bool BLI_uniquename_cb(UniquenameCheckCallback unique_check,
                       void *arg,
                       const char *defname,
                       char delim,
                       char *name,
                       size_t name_len);
/**
 * Ensures that the specified block has a unique name within the containing list,
 * incrementing its numeric suffix as necessary. Returns true if name had to be adjusted.
 *
 * \param list: List containing the block
 * \param vlink: The block to check the name for
 * \param defname: To initialize block name if latter is empty
 * \param delim: Delimits numeric suffix in name
 * \param name_offset: Offset of name within block structure
 * \param name_len: Maximum length of name area
 */
bool BLI_uniquename(struct ListBase *list,
                    void *vlink,
                    const char *defname,
                    char delim,
                    int name_offset,
                    size_t name_len);

#ifdef __cplusplus
}
#endif
