/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_STRING_UTILS_H__
#define __BLI_STRING_UTILS_H__

/** \file BLI_string_utils.h
 *  \ingroup bli
 */

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"

struct ListBase;

typedef bool (*UniquenameCheckCallback)(void *arg, const char *name);

size_t BLI_split_name_num(char *left, int *nr, const char *name, const char delim);

void BLI_string_split_suffix(const char *string, char *r_body, char *r_suf, const size_t str_len);
void BLI_string_split_prefix(const char *string, char *r_pre, char *r_body, const size_t str_len);

void BLI_string_flip_side_name(char *r_name, const char *from_name, const bool strip_number, const size_t name_len);

bool BLI_uniquename_cb(
        UniquenameCheckCallback unique_check, void *arg, const char *defname, char delim, char *name, size_t name_len);
bool BLI_uniquename(
        struct ListBase *list, void *vlink, const char *defname, char delim, int name_offs, size_t len);

#ifdef __cplusplus
}
#endif

#endif  /* __BLI_STRING_UTILS_H__ */
