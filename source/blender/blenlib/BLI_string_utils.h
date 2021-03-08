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

size_t BLI_split_name_num(char *left, int *nr, const char *name, const char delim);
bool BLI_string_is_decimal(const char *string) ATTR_NONNULL();

void BLI_string_split_suffix(const char *string, char *r_body, char *r_suf, const size_t str_len);
void BLI_string_split_prefix(const char *string, char *r_pre, char *r_body, const size_t str_len);

/* Join strings, return newly allocated string. */
char *BLI_string_join_array(char *result,
                            size_t result_len,
                            const char *strings[],
                            uint strings_len) ATTR_NONNULL();
char *BLI_string_join_array_by_sep_char(char *result,
                                        size_t result_len,
                                        char sep,
                                        const char *strings[],
                                        uint strings_len) ATTR_NONNULL();

char *BLI_string_join_arrayN(const char *strings[], uint strings_len) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
char *BLI_string_join_array_by_sep_charN(char sep,
                                         const char *strings[],
                                         uint strings_len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
char *BLI_string_join_array_by_sep_char_with_tableN(char sep,
                                                    char *table[],
                                                    const char *strings[],
                                                    uint strings_len) ATTR_NONNULL();
/* Take multiple arguments, pass as (array, length). */
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

void BLI_string_flip_side_name(char *r_name,
                               const char *from_name,
                               const bool strip_number,
                               const size_t name_len);

bool BLI_uniquename_cb(UniquenameCheckCallback unique_check,
                       void *arg,
                       const char *defname,
                       char delim,
                       char *name,
                       size_t name_len);
bool BLI_uniquename(struct ListBase *list,
                    void *vlink,
                    const char *defname,
                    char delim,
                    int name_offset,
                    size_t len);

#ifdef __cplusplus
}
#endif
